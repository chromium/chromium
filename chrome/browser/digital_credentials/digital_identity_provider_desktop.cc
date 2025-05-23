// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/digital_credentials/digital_identity_provider_desktop.h"

#include <memory>
#include <variant>

#include "base/containers/span.h"
#include "base/functional/overloaded.h"
#include "base/values.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/digital_credentials/digital_identity_low_risk_origins.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_bluetooth_manual_dialog_controller.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_multi_step_dialog.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_safety_interstitial_controller_desktop.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "content/public/browser/cross_device_request_info.h"
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
#include "ui/views/controls/theme_tracking_animated_image_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/widget.h"

namespace {

// Smaller than DistanceMetric::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH.
constexpr int kQrCodeSize = 240;

using DigitalIdentityInterstitialAbortCallback =
    content::DigitalIdentityProvider::DigitalIdentityInterstitialAbortCallback;
using RequestStatusForMetrics =
    content::DigitalIdentityProvider::RequestStatusForMetrics;
using content::digital_credentials::cross_device::Error;
using content::digital_credentials::cross_device::Event;
using content::digital_credentials::cross_device::ProtocolError;
using content::digital_credentials::cross_device::RemoteError;
using content::digital_credentials::cross_device::RequestInfo;
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
  return image_view;
}

device::cablev2::CredentialRequestType
CrossDeviceRequestTypeToCredentialRequestType(
    RequestInfo::RequestType request_type) {
  switch (request_type) {
    case RequestInfo::RequestType::kGet:
      return device::cablev2::CredentialRequestType::kPresentation;
    case RequestInfo::RequestType::kCreate:
      return device::cablev2::CredentialRequestType::kIssuance;
  }
}

}  // anonymous namespace

DigitalIdentityProviderDesktop::DigitalIdentityProviderDesktop() = default;

DigitalIdentityProviderDesktop::~DigitalIdentityProviderDesktop() = default;

bool DigitalIdentityProviderDesktop::IsLowRiskOrigin(
    content::RenderFrameHost& render_frame_host) const {
  return digital_credentials::IsLowRiskOrigin(render_frame_host);
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

void DigitalIdentityProviderDesktop::Get(content::WebContents* web_contents,
                                         const url::Origin& rp_origin,
                                         base::ValueView request,
                                         DigitalIdentityCallback callback) {
  Transact(web_contents, RequestInfo::RequestType::kGet, rp_origin, request,
           std::move(callback));
}

void DigitalIdentityProviderDesktop::Create(content::WebContents* web_contents,
                                            const url::Origin& rp_origin,
                                            base::ValueView request,
                                            DigitalIdentityCallback callback) {
  Transact(web_contents, RequestInfo::RequestType::kCreate, rp_origin, request,
           std::move(callback));
}

void DigitalIdentityProviderDesktop::Transact(
    content::WebContents* web_contents,
    RequestInfo::RequestType request_type,
    const url::Origin& rp_origin,
    base::ValueView request,
    DigitalIdentityCallback callback) {
  web_contents_ = web_contents->GetWeakPtr();
  rp_origin_ = rp_origin;
  callback_ = std::move(callback);

  RequestInfo request_info{request_type, rp_origin, request.ToValue()};

  std::array<uint8_t, device::cablev2::kQRKeySize> qr_generator_key;
  crypto::RandBytes(qr_generator_key);

  std::string qr_url = device::cablev2::qr::Encode(
      qr_generator_key,
      CrossDeviceRequestTypeToCredentialRequestType(request_type));

  transaction_ = Transaction::New(
      std::move(request_info), qr_generator_key, base::BindRepeating([]() {
        return SystemNetworkContextManager::GetInstance()->GetContext();
      }),
      base::BindRepeating(&DigitalIdentityProviderDesktop::OnEvent,
                          weak_ptr_factory_.GetWeakPtr(), std::move(qr_url)),
      base::BindOnce(&DigitalIdentityProviderDesktop::OnFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DigitalIdentityProviderDesktop::OnEvent(const std::string& qr_url,
                                             Event event) {
  std::visit(base::Overloaded{
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
                 [this](device::cablev2::Event event) { OnCableEvent(event); },
             },
             event);
}

void DigitalIdentityProviderDesktop::OnCableEvent(
    device::cablev2::Event event) {
  switch (event) {
    case device::cablev2::Event::kPhoneConnected:
    case device::cablev2::Event::kBLEAdvertReceived:
      ShowConnectingToPhoneDialog();
      if (!cable_connecting_dialog_timer_.IsRunning()) {
        cable_connecting_dialog_timer_.Start(
            FROM_HERE, base::Milliseconds(2500),
            base::BindOnce(
                &DigitalIdentityProviderDesktop::OnCableConnectingTimerComplete,
                weak_ptr_factory_.GetWeakPtr()));
      }
      break;
    case device::cablev2::Event::kReady:
      // If we are ready before the timer fires, don't show the next dialog
      // directly to make sure the "connecting to phone" dialog is visible for
      // long enough time to avoid flashing the UI. Otherwise, show the next
      // dialog directly.
      if (cable_connecting_dialog_timer_.IsRunning()) {
        cable_connecting_ready_to_advance_ = true;
      } else {
        ShowContinueStepsOnThePhoneDialog();
      }
      break;
  }
}

void DigitalIdentityProviderDesktop::OnFinished(
    base::expected<Response, Error> result) {
  if (result.has_value()) {
    std::move(callback_).Run(std::move(result.value().value()));
    return;
  }

  std::visit(
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
  std::u16string dialog_body =
      l10n_util::GetStringUTF16(IDS_WEB_DIGITAL_CREDENTIALS_QR_BODY);
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

void DigitalIdentityProviderDesktop::ShowConnectingToPhoneDialog() {
  // Ensure the dialog is created to have access to GetBackgroundColor().
  EnsureDialogCreated();
  std::u16string title_text = l10n_util::GetStringUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_CABLEV2_CONNECTING_TITLE);
  auto illustration = std::make_unique<views::ThemeTrackingAnimatedImageView>(
      IDR_WEBAUTHN_HYBRID_CONNECTING_LIGHT, IDR_WEBAUTHN_HYBRID_CONNECTING_DARK,
      base::BindRepeating(&DigitalIdentityMultiStepDialog::GetBackgroundColor,
                          base::Unretained(dialog_.get())));

  EnsureDialogCreated()->TryShow(
      /*accept_button=*/std::nullopt, base::OnceClosure(),
      ui::DialogModel::Button::Params(),
      base::BindOnce(&DigitalIdentityProviderDesktop::OnCanceled,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(title_text), /*dialog_body=*/u"",
      DigitalIdentityMultiStepDialog::ConfigureHeaderIllustration(
          std::move(illustration)));
}

void DigitalIdentityProviderDesktop::ShowContinueStepsOnThePhoneDialog() {
  // Ensure the dialog is created to have access to GetBackgroundColor().
  EnsureDialogCreated();
  std::u16string title_text = l10n_util::GetStringUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_CABLEV2_CONNECTED_TITLE);
  auto illustration = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      ui::ImageModel::FromVectorIcon(kPasskeyPhoneIcon),
      ui::ImageModel::FromVectorIcon(kPasskeyPhoneDarkIcon),
      base::BindRepeating(&DigitalIdentityMultiStepDialog::GetBackgroundColor,
                          base::Unretained(dialog_.get())));

  EnsureDialogCreated()->TryShow(
      /*accept_button=*/std::nullopt, base::OnceClosure(),
      ui::DialogModel::Button::Params(),
      base::BindOnce(&DigitalIdentityProviderDesktop::OnCanceled,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(title_text), /*dialog_body=*/u"",
      DigitalIdentityMultiStepDialog::ConfigureHeaderIllustration(
          std::move(illustration)));
}

void DigitalIdentityProviderDesktop::OnCableConnectingTimerComplete() {
  if (cable_connecting_ready_to_advance_) {
    ShowContinueStepsOnThePhoneDialog();
  }
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
