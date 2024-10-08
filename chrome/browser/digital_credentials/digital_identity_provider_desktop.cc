// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/digital_credentials/digital_identity_provider_desktop.h"

#include <memory>

#include "base/containers/span.h"
#include "base/functional/overloaded.h"
#include "base/json/json_writer.h"
#include "chrome/browser/digital_credentials/digital_identity_low_risk_origins.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_bluetooth_manual_dialog_controller.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_multi_step_dialog.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_safety_interstitial_controller_desktop.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/digital_credentials_cross_device.h"
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

// Smaller than DistanceMetric::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH.
const int kQrCodeSize = 240;

using DigitalIdentityInterstitialAbortCallback =
    content::DigitalIdentityProvider::DigitalIdentityInterstitialAbortCallback;
using RequestStatusForMetrics =
    content::DigitalIdentityProvider::RequestStatusForMetrics;
using content::digital_credentials::cross_device::Error;
using content::digital_credentials::cross_device::Event;
using content::digital_credentials::cross_device::ProtocolError;
using content::digital_credentials::cross_device::RemoteError;
using content::digital_credentials::cross_device::Response;
using content::digital_credentials::cross_device::SystemError;
using content::digital_credentials::cross_device::SystemEvent;
using content::digital_credentials::cross_device::Transaction;

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

DigitalIdentityProviderDesktop::~DigitalIdentityProviderDesktop() = default;

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
                                             base::Value request,
                                             DigitalIdentityCallback callback) {
  web_contents_ = web_contents->GetWeakPtr();
  rp_origin_ = rp_origin;
  callback_ = std::move(callback);

  std::array<uint8_t, device::cablev2::kQRKeySize> qr_generator_key;
  crypto::RandBytes(qr_generator_key);

  std::string qr_url = device::cablev2::qr::Encode(
      qr_generator_key, device::cablev2::CredentialRequestType::kPresentation);

  transaction_ = Transaction::New(
      rp_origin, std::move(request), qr_generator_key,
      base::BindRepeating([]() {
        return SystemNetworkContextManager::GetInstance()->GetContext();
      }),
      base::BindRepeating(&DigitalIdentityProviderDesktop::OnEvent,
                          weak_ptr_factory_.GetWeakPtr(), std::move(qr_url)),
      base::BindOnce(&DigitalIdentityProviderDesktop::OnFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DigitalIdentityProviderDesktop::OnEvent(const std::string& qr_url,
                                             Event event) {
  absl::visit(base::Overloaded{
                  [this, qr_url](SystemEvent event) {
                    switch (event) {
                      case SystemEvent::kBluetoothNotPowered:
                        ShowBluetoothManualTurnOnDialog();
                        break;
                      case SystemEvent::kNeedPermission:
                        // The user is being asked for Bluetooth permission by
                        // the system. Nothing for Chrome UI to do.
                        break;
                      case SystemEvent::kReady:
                        bluetooth_manual_dialog_controller_.reset();
                        ShowQrCodeDialog(qr_url);
                        break;
                    }
                  },
                  [](device::cablev2::Event event) {
                    // caBLE events notify when the user has started the
                    // transaction on their phone. The desktop UI could update
                    // at this point to instruct the user to complete the action
                    // there.
                  },
              },
              event);
}

void DigitalIdentityProviderDesktop::OnFinished(
    base::expected<Response, Error> result) {
  if (result.has_value()) {
    std::string encoded_result =
        base::WriteJsonWithOptions(result.value().value(),
                                   base::JSONWriter::OPTIONS_PRETTY_PRINT)
            .value_or("");
    std::move(callback_).Run(std::move(encoded_result));
    return;
  }

  absl::visit(
      base::Overloaded{
          [this](SystemError error) {
            EndRequestWithError(RequestStatusForMetrics::kErrorOther);
          },
          [this](ProtocolError error) {
            EndRequestWithError(RequestStatusForMetrics::kErrorOther);
          },
          [this](RemoteError error) {
            switch (error) {
              case RemoteError::kNoCredential:
                EndRequestWithError(
                    RequestStatusForMetrics::kErrorNoCredential);
                break;
              case RemoteError::kUserCanceled:
                EndRequestWithError(
                    RequestStatusForMetrics::kErrorUserDeclined);
                break;
              case RemoteError::kDeviceAborted:
                EndRequestWithError(RequestStatusForMetrics::kErrorAborted);
                break;
              case RemoteError::kOther:
                EndRequestWithError(RequestStatusForMetrics::kErrorOther);
                break;
            }
          }},
      result.error());
}

DigitalIdentityMultiStepDialog*
DigitalIdentityProviderDesktop::EnsureDialogCreated() {
  if (!dialog_) {
    dialog_ = std::make_unique<DigitalIdentityMultiStepDialog>(web_contents_);
  }
  return dialog_.get();
}

void DigitalIdentityProviderDesktop::ShowQrCodeDialog(
    const std::string& qr_url) {
  std::u16string dialog_title =
      l10n_util::GetStringUTF16(IDS_WEB_DIGITAL_CREDENTIALS_QR_TITLE);
  std::u16string dialog_body = l10n_util::GetStringFUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_QR_BODY,
      url_formatter::FormatOriginForSecurityDisplay(
          rp_origin_, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  EnsureDialogCreated()->TryShow(
      /*accept_button=*/std::nullopt, base::OnceClosure(),
      ui::DialogModel::Button::Params(),
      base::BindOnce(&DigitalIdentityProviderDesktop::OnCanceled,
                     weak_ptr_factory_.GetWeakPtr()),
      dialog_title, dialog_body, MakeQrCodeImageView(qr_url));
}

void DigitalIdentityProviderDesktop::ShowBluetoothManualTurnOnDialog() {
  bluetooth_manual_dialog_controller_ =
      std::make_unique<DigitalIdentityBluetoothManualDialogController>(
          EnsureDialogCreated());
  bluetooth_manual_dialog_controller_->Show(
      base::BindOnce(
          &DigitalIdentityProviderDesktop::OnUserRequestedBluetoothPowerOn,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DigitalIdentityProviderDesktop::OnCanceled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DigitalIdentityProviderDesktop::OnUserRequestedBluetoothPowerOn() {
  transaction_->PowerBluetoothAdapter();
}

void DigitalIdentityProviderDesktop::OnCanceled() {
  EndRequestWithError(RequestStatusForMetrics::kErrorOther);
}

void DigitalIdentityProviderDesktop::EndRequestWithError(
    RequestStatusForMetrics status) {
  if (callback_.is_null()) {
    return;
  }

  bluetooth_manual_dialog_controller_.reset();
  dialog_.reset();

  std::move(callback_).Run(base::unexpected(status));
}
