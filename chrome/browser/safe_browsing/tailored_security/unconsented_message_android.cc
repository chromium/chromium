// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/unconsented_message_android.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/android/safe_browsing_settings_launcher_android.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_outcome.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
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
#include "ui/views/image_model_utils.h"

namespace safe_browsing {

namespace {

void LogMessageOutcome(TailoredSecurityOutcome outcome) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.TailoredSecurityUnconsentedMessageOutcome", outcome);
}

const int kAvatarSize = 256;
const int kAvatarWithBorderSize = 300;
const int kBadgeSize = 100;

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

TailoredSecurityUnconsentedModalAndroid::
    TailoredSecurityUnconsentedModalAndroid(content::WebContents* web_contents,
                                            base::OnceClosure dismiss_callback_)
    : dismiss_callback_(std::move(dismiss_callback_)),
      web_contents_(web_contents) {
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::TAILORED_SECURITY_ENABLED,
      base::BindOnce(
          &TailoredSecurityUnconsentedModalAndroid::HandleMessageAccepted,
          base::Unretained(this)),
      base::BindOnce(
          &TailoredSecurityUnconsentedModalAndroid::HandleMessageDismissed,
          base::Unretained(this)));

  message_->SetTitle(l10n_util::GetStringUTF16(
      IDS_TAILORED_SECURITY_UNCONSENTED_MESSAGE_TITLE));
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(
      IDS_TAILORED_SECURITY_UNCONSENTED_MESSAGE_ACCEPT));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (identity_manager &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    gfx::ImageSkia avatar_image = identity_manager
                                      ->FindExtendedAccountInfoByAccountId(
                                          identity_manager->GetPrimaryAccountId(
                                              signin::ConsentLevel::kSignin))
                                      .account_image.AsImageSkia();

    gfx::ImageSkia sized_avatar_image =
        gfx::ImageSkiaOperations::CreateResizedImage(
            avatar_image, skia::ImageOperations::RESIZE_BEST,
            gfx::Size(kAvatarSize, kAvatarSize));
    gfx::ImageSkia cropped_avatar_image =
        gfx::ImageSkiaOperations::CreateMaskedImage(
            sized_avatar_image,
            gfx::CanvasImageSource::MakeImageSkia<CircleImageSource>(
                sized_avatar_image.width(), SK_ColorWHITE));
    gfx::ImageSkia final_avatar_image =
        gfx::ImageSkiaOperations::CreateSuperimposedImage(
            gfx::CanvasImageSource::MakeImageSkia<CircleImageSource>(
                kAvatarWithBorderSize, gfx::kGoogleBlue400),
            cropped_avatar_image);
    gfx::ImageSkia badge = gfx::CreateVectorIcon(kSafetyCheckIcon, kBadgeSize,
                                                 gfx::kGoogleBlue500);
    icon_ = gfx::ImageSkiaOperations::CreateIconWithBadge(final_avatar_image,
                                                          badge);
    message_->SetIcon(*icon_.bitmap());
    message_->DisableIconTint();
  }

  message_->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS));
  message_->SetSecondaryActionCallback(base::BindOnce(
      &TailoredSecurityUnconsentedModalAndroid::HandleSettingsClicked,
      base::Unretained(this)));

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents_, messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);
}

TailoredSecurityUnconsentedModalAndroid::
    ~TailoredSecurityUnconsentedModalAndroid() {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::UNKNOWN);
    if (dismiss_callback_)
      std::move(dismiss_callback_).Run();
  }
}

void TailoredSecurityUnconsentedModalAndroid::HandleMessageAccepted() {
  LogMessageOutcome(TailoredSecurityOutcome::kAccepted);

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  SetSafeBrowsingState(profile->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION,
                       /*is_esb_enabled_in_sync=*/false);
  ShowSafeBrowsingSettings(web_contents_);
}

void TailoredSecurityUnconsentedModalAndroid::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  LogMessageOutcome(TailoredSecurityOutcome::kDismissed);
  base::UmaHistogramEnumeration(
      "SafeBrowsing.TailoredSecurityUnconsentedMessageDismissReason",
      dismiss_reason, messages::DismissReason::COUNT);
  message_.reset();
  // `dismiss_callback_` may delete `this`.
  if (dismiss_callback_)
    std::move(dismiss_callback_).Run();
}

void TailoredSecurityUnconsentedModalAndroid::HandleSettingsClicked() {
  LogMessageOutcome(TailoredSecurityOutcome::kSettings);
  ShowSafeBrowsingSettings(web_contents_);
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::SECONDARY_ACTION);
    // `dismiss_callback_` may delete `this`.
    if (dismiss_callback_)
      std::move(dismiss_callback_).Run();
  }
}

}  // namespace safe_browsing
