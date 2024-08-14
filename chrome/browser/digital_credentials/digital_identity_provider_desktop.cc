// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/digital_credentials/digital_identity_provider_desktop.h"

#include <memory>

#include "base/containers/span.h"
#include "chrome/browser/digital_credentials/digital_identity_fido_handler_observer.h"
#include "chrome/browser/digital_credentials/digital_identity_low_risk_origins.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_bluetooth_manual_dialog_controller.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_multi_step_dialog.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_safety_interstitial_controller_desktop.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/browser/web_contents.h"
#include "crypto/random.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_handshake.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace {

using BleStatus = device::FidoRequestHandlerBase::BleStatus;

// Smaller than DistanceMetric::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH.
const int kQrCodeSize = 240;

using DigitalIdentityInterstitialAbortCallback =
    content::DigitalIdentityProvider::DigitalIdentityInterstitialAbortCallback;
using RequestStatusForMetrics =
    content::DigitalIdentityProvider::RequestStatusForMetrics;
using TransportAvailabilityInfo =
    device::FidoRequestHandlerBase::TransportAvailabilityInfo;

void RunDigitalIdentityCallback(
    std::unique_ptr<DigitalIdentitySafetyInterstitialControllerDesktop>
        controller,
    content::DigitalIdentityProvider::DigitalIdentityInterstitialCallback
        callback,
    content::DigitalIdentityProvider::RequestStatusForMetrics
        status_for_metrics) {
  std::move(callback).Run(status_for_metrics);
}

std::unique_ptr<views::View> MakeQrCodeImageView(const std::string& qr_url) {
  auto qr_code = qr_code_generator::GenerateImage(
      base::as_byte_span(qr_url), qr_code_generator::ModuleStyle::kCircles,
      qr_code_generator::LocatorStyle::kRounded,
      qr_code_generator::CenterImage::kNoCenterImage,
      qr_code_generator::QuietZone::kIncluded);

  // Success is guaranteed, because `qr_url`'s size is bounded and smaller
  // than QR code limits.
  CHECK(qr_code.has_value());
  auto image_view = std::make_unique<views::ImageView>(
      ui::ImageModel::FromImageSkia(qr_code.value()));
  image_view->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_WEB_DIGITAL_CREDENTIALS_QR_CODE_ALT_TEXT));
  image_view->SetImageSize(gfx::Size(kQrCodeSize, kQrCodeSize));
  return std::move(image_view);
}

}  // anonymous namespace

DigitalIdentityProviderDesktop::DigitalIdentityProviderDesktop() = default;

DigitalIdentityProviderDesktop::~DigitalIdentityProviderDesktop() {
  // Destroy members with raw_ptrs to `request_handler_observer_`.
  bluetooth_manual_dialog_controller_.reset();
  request_handler_.reset();

  request_handler_observer_.reset();
}

bool DigitalIdentityProviderDesktop::IsLowRiskOrigin(
    const url::Origin& to_check) const {
  return digital_credentials::IsLowRiskOrigin(to_check);
}

DigitalIdentityInterstitialAbortCallback
DigitalIdentityProviderDesktop::ShowDigitalIdentityInterstitial(
    content::WebContents& web_contents,
    const url::Origin& origin,
    content::DigitalIdentityInterstitialType interstitial_type,
    DigitalIdentityInterstitialCallback callback) {
  auto controller =
      std::make_unique<DigitalIdentitySafetyInterstitialControllerDesktop>();
  // Callback takes ownership of |controller|.
  return controller->ShowInterstitial(
      web_contents, origin, interstitial_type,
      base::BindOnce(&RunDigitalIdentityCallback, std::move(controller),
                     std::move(callback)));
}

void DigitalIdentityProviderDesktop::Request(content::WebContents* web_contents,
                                             const url::Origin& rp_origin,
                                             const std::string& request,
                                             DigitalIdentityCallback callback) {
  web_contents_ = web_contents->GetWeakPtr();
  rp_origin_ = rp_origin;
  callback_ = std::move(callback);

  const auto fido_request_type = device::FidoRequestType::kGetAssertion;
  std::array<uint8_t, device::cablev2::kQRKeySize> qr_generator_key;
  crypto::RandBytes(qr_generator_key);

  discovery_factory_ = std::make_unique<device::FidoDiscoveryFactory>();
  discovery_factory_->set_cable_data(fido_request_type, {}, qr_generator_key);
  discovery_factory_->set_network_context_factory(base::BindRepeating([]() {
    return SystemNetworkContextManager::GetInstance()->GetContext();
  }));

  qr_url_ = device::cablev2::qr::Encode(qr_generator_key, fido_request_type);

  request_handler_ = std::make_unique<device::DigitalIdentityRequestHandler>(
      discovery_factory_.get());
  request_handler_observer_ =
      std::make_unique<DigitalIdentityFidoHandlerObserver>(
          base::BindOnce(&DigitalIdentityProviderDesktop::OnReadyToShowUi,
                         weak_ptr_factory_.GetWeakPtr()));
  request_handler_->set_observer(request_handler_observer_.get());
}

void DigitalIdentityProviderDesktop::OnReadyToShowUi(
    const TransportAvailabilityInfo& availability_info) {
  if (availability_info.ble_status == BleStatus::kOn) {
    ShowQrCodeDialog();
    return;
  }

  ShowBluetoothManualTurnOnDialog();
}

DigitalIdentityMultiStepDialog*
DigitalIdentityProviderDesktop::EnsureDialogCreated() {
  if (!dialog_) {
    dialog_ = std::make_unique<DigitalIdentityMultiStepDialog>(web_contents_);
  }
  return dialog_.get();
}

void DigitalIdentityProviderDesktop::ShowQrCodeDialog() {
  std::u16string dialog_title =
      l10n_util::GetStringUTF16(IDS_WEB_DIGITAL_CREDENTIALS_QR_TITLE);
  std::u16string dialog_body = l10n_util::GetStringFUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_QR_BODY,
      url_formatter::FormatOriginForSecurityDisplay(
          rp_origin_, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  EnsureDialogCreated()->TryShow(
      /*ok_button=*/std::nullopt, base::OnceClosure(),
      ui::DialogModel::Button::Params(),
      base::BindOnce(&DigitalIdentityProviderDesktop::OnCanceled,
                     weak_ptr_factory_.GetWeakPtr()),
      dialog_title, dialog_body, MakeQrCodeImageView(qr_url_));
}

void DigitalIdentityProviderDesktop::ShowBluetoothManualTurnOnDialog() {
  bluetooth_manual_dialog_controller_ =
      std::make_unique<DigitalIdentityBluetoothManualDialogController>(
          EnsureDialogCreated(), request_handler_observer_.get());
  bluetooth_manual_dialog_controller_->Show(
      base::BindRepeating(&DigitalIdentityProviderDesktop::OnBluetoothTurnedOn,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&DigitalIdentityProviderDesktop::OnCanceled,
                          weak_ptr_factory_.GetWeakPtr()));
}

void DigitalIdentityProviderDesktop::OnBluetoothTurnedOn() {
  bluetooth_manual_dialog_controller_.reset();
  ShowQrCodeDialog();
}

void DigitalIdentityProviderDesktop::OnCanceled() {
  if (callback_.is_null()) {
    return;
  }

  bluetooth_manual_dialog_controller_.reset();
  dialog_ = nullptr;
  std::move(callback_).Run(
      base::unexpected(RequestStatusForMetrics::kErrorOther));
}
