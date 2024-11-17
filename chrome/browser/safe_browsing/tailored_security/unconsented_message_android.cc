// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/unconsented_message_android.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/android/safe_browsing_settings_navigation_android.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_outcome.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"

namespace safe_browsing {

namespace {

void LogMessageOutcome(TailoredSecurityOutcome outcome, bool is_in_flow) {
  if (is_in_flow) {
    base::UmaHistogramEnumeration(
        "SafeBrowsing.TailoredSecurityUnconsentedInFlowMessageOutcome",
        outcome);
  } else {
    base::UmaHistogramEnumeration(
        "SafeBrowsing.TailoredSecurityUnconsentedOutOfFlowMessageOutcome",
        outcome);
  }
}

class CircleImageSource : public gfx::CanvasImageSource {
 public:
  CircleImageSource(int size, SkColor color)
      : gfx::CanvasImageSource(gfx::Size(size, size)), color_(color) {}

  CircleImageSource(const CircleImageSource&) = delete;
  CircleImageSource& operator=(const CircleImageSource&) = delete;

  ~CircleImageSource() override = default;

  void Draw(gfx::Canvas* canvas) override;

 private:
  SkColor color_;
};

void CircleImageSource::Draw(gfx::Canvas* canvas) {
  float radius = size().width() / 2.0f;
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(color_);
  canvas->DrawCircle(gfx::PointF(radius, radius), radius, flags);
}

}  // namespace

TailoredSecurityUnconsentedMessageAndroid::
    TailoredSecurityUnconsentedMessageAndroid(
        content::WebContents* web_contents,
        base::OnceClosure dismiss_callback,
        bool is_in_flow)
    : dismiss_callback_(std::move(dismiss_callback)),
      web_contents_(web_contents),
      is_in_flow_(is_in_flow) {
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::TAILORED_SECURITY_ENABLED,
      base::BindOnce(
          &TailoredSecurityUnconsentedMessageAndroid::HandleMessageAccepted,
          base::Unretained(this)),
      base::BindOnce(
          &TailoredSecurityUnconsentedMessageAndroid::HandleMessageDismissed,
          base::Unretained(this)));

  int message_title =
      is_in_flow_ ? IDS_TAILORED_SECURITY_UNCONSENTED_MESSAGE_TITLE
                  : IDS_TAILORED_SECURITY_UNCONSENTED_PROMOTION_MESSAGE_TITLE;
  int primary_button =
      is_in_flow_ ? IDS_TAILORED_SECURITY_UNCONSENTED_MESSAGE_ACCEPT
                  : IDS_TAILORED_SECURITY_UNCONSENTED_PROMOTION_MESSAGE_ACCEPT;
  message_->SetTitle(l10n_util::GetStringUTF16(message_title));
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(primary_button));
  if (!is_in_flow_) {
    message_->SetDescription(l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_UNCONSENTED_PROMOTION_MESSAGE_DESCRIPTION));
  }

  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SHIELD_BLUE));
  // Need to disable tint here because it removes a shade of blue from the
  // shield which distorts the image.
  message_->DisableIconTint();

  LogMessageOutcome(TailoredSecurityOutcome::kShown, is_in_flow_);
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents_, messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);
}

TailoredSecurityUnconsentedMessageAndroid::
    ~TailoredSecurityUnconsentedMessageAndroid() {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::UNKNOWN);
    if (dismiss_callback_)
      std::move(dismiss_callback_).Run();
  }
}

void TailoredSecurityUnconsentedMessageAndroid::HandleMessageAccepted() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (is_in_flow_) {
    LogMessageOutcome(TailoredSecurityOutcome::kAccepted, is_in_flow_);
    SetSafeBrowsingState(profile->GetPrefs(),
                         SafeBrowsingState::ENHANCED_PROTECTION);
  } else {
    LogMessageOutcome(TailoredSecurityOutcome::kSettings, is_in_flow_);
  }

  ShowSafeBrowsingSettings(web_contents_->GetTopLevelNativeWindow(),
                           SettingsAccessPoint::kTailoredSecurity);
}

void TailoredSecurityUnconsentedMessageAndroid::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  LogMessageOutcome(TailoredSecurityOutcome::kDismissed, is_in_flow_);
  message_.reset();
  // `dismiss_callback_` may delete `this`.
  if (dismiss_callback_)
    std::move(dismiss_callback_).Run();
}

}  // namespace safe_browsing
