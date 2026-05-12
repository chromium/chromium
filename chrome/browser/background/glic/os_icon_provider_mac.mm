// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/os_icon_provider_mac.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/background/glic/glic_status_icon.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/paint_vector_icon.h"

@interface RunningAppsListener : NSObject
@property(readonly, nonatomic) glic::OSIconProviderMac* iconProvider;
@end

@implementation RunningAppsListener

@synthesize iconProvider = _iconProvider;
static void* kKVOContext = (void*)&kKVOContext;

- (instancetype)initWithIconProvider:(glic::OSIconProviderMac*)icon_provider {
  self = [super init];
  if (self) {
    _iconProvider = icon_provider;

    // Watch updates to the runningApplications list with a key-value
    // observation. Handling NSWorkspaceDidLaunchApplicationNotification isn't
    // sufficient because we need to detect background apps too (those with
    // LSUIElement=true).
    //
    // Also note that this observation triggers both for app launches and
    // terminations. We only care about launches for updating the glic status
    // tray icon, but it's okay to run on terminations too.
    [[NSWorkspace sharedWorkspace] addObserver:self
                                    forKeyPath:@"runningApplications"
                                       options:NSKeyValueObservingOptionNew
                                       context:kKVOContext];
  }
  return self;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if (context != kKVOContext) {
    [super observeValueForKeyPath:keyPath
                         ofObject:object
                           change:change
                          context:context];
    return;
  }

  if (!self.iconProvider || ![keyPath isEqualToString:@"runningApplications"]) {
    return;
  }

  for (NSRunningApplication* app in
       [[NSWorkspace sharedWorkspace] runningApplications]) {
    if (app.bundleIdentifier) {
      self.iconProvider->OnRunningAppsUpdated(
          base::SysNSStringToUTF8(app.bundleIdentifier));
    }
  }
}

- (void)dealloc {
  [[NSWorkspace sharedWorkspace] removeObserver:self
                                     forKeyPath:@"runningApplications"
                                        context:kKVOContext];
}
@end

namespace glic {
namespace {

class DelegateImpl : public OSIconProviderMac::Delegate {
 public:
  explicit DelegateImpl(OSIconProviderMac* icon_provider)
      : listener_(
            [[RunningAppsListener alloc] initWithIconProvider:icon_provider]) {}
  ~DelegateImpl() override {
    // Explicitly set nil to trigger autorelease.
    listener_ = nil;
  }

  bool IsAppRunning(const std::string& bundle_id) const override {
    return [[NSRunningApplication
        runningApplicationsWithBundleIdentifier:base::SysUTF8ToNSString(
                                                    bundle_id)] count];
  }

 private:
  // Allocated in constructor and deallocated by auto release. raw_ptr doesn't
  // work with this type.
  RunningAppsListener* listener_ = nullptr;
};

// If true, watch for an app running or starting up with the bundle identifier
// specified by kGlicChromeStatusIconOtherAppID and switch to the alt icon if
// that app is running.
bool DynamicIconUpdateEnabled() {
  return base::FeatureList::IsEnabled(features::kGlicChromeStatusIcon) &&
         !features::kGlicChromeStatusIconOtherAppID.Get().empty();
}

}  // namespace

OSIconProviderMac::OSIconProviderMac(PrefService& prefs,
                                     GlicStatusIcon& glic_status_icon)
    : OSIconProviderMac(prefs,
                        glic_status_icon,
                        DynamicIconUpdateEnabled()
                            ? std::make_unique<DelegateImpl>(this)
                            : nullptr) {}

OSIconProviderMac::OSIconProviderMac(PrefService& prefs,
                                     GlicStatusIcon& glic_status_icon,
                                     std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)),
      prefs_(prefs),
      glic_status_icon_(glic_status_icon) {
  if (!DynamicIconUpdateEnabled()) {
    // Reset the alt icon stickiness pref if either the feature flag or the
    // dynamic update is disabled.
    prefs_->ClearPref(prefs::kGlicUseAltOSIcon);
  }

  if (base::FeatureList::IsEnabled(features::kGlicChromeStatusIcon)) {
    if (DynamicIconUpdateEnabled()) {
      // Check whether the icon needs to change because the app with the
      // specified bundle ID is running.
      const bool should_change_icon =
          !GetUseAltIcon() &&
          delegate_->IsAppRunning(
              features::kGlicChromeStatusIconOtherAppID.Get());
      base::UmaHistogramBoolean("Glic.SetAltOSIcon.OnChromeStart",
                                should_change_icon);
      if (should_change_icon && !features::kGlicChromeStatusIconLogOnly.Get()) {
        SetUseAltIcon(true);
      }
    } else {
      // If GlicChromeStatusIcon is enabled without dynamic update, always use
      // the alt icon.
      SetUseAltIcon(true);
    }
  }
}

OSIconProviderMac::~OSIconProviderMac() = default;

gfx::ImageSkia OSIconProviderMac::GetIcon() const {
  if (GetUseAltIcon() && !features::kGlicChromeStatusIconLogOnly.Get()) {
    // Despite the similar name, this feature param now decides which alt icon
    // to use, not whether to use an alt icon at all.
    if (features::kGlicChromeStatusIconUseAltIcon.Get()) {
      return gfx::CreateVectorIcon(glic::GlicVectorIconManager::GetVectorIcon(
                                       IDR_GLIC_MAC_ALT_STATUS_ICON),
                                   features::kGlicChromeStatusIconSizePx.Get(),
                                   SK_ColorWHITE);
    }
    return gfx::CreateVectorIcon(omnibox::kProductChromeRefreshIcon,
                                 features::kGlicChromeStatusIconSizePx.Get(),
                                 SK_ColorWHITE);
  }
  const auto& icon =
      glic::GlicVectorIconManager::GetVectorIcon(IDR_GLIC_STATUS_ICON);
  return gfx::CreateVectorIcon(icon, SK_ColorWHITE);
}

void OSIconProviderMac::SetUseAltIcon(bool use_alt_icon) {
  if (use_alt_icon == GetUseAltIcon()) {
    return;
  }
  prefs_->SetBoolean(prefs::kGlicUseAltOSIcon, use_alt_icon);

  // Recreate the status tray icon using the updated icon.
  glic_status_icon_->SetIcon(GetIcon());
}

bool OSIconProviderMac::GetUseAltIcon() const {
  return prefs_->GetBoolean(prefs::kGlicUseAltOSIcon);
}

void OSIconProviderMac::OnRunningAppsUpdated(const std::string& bundle_id) {
  if (bundle_id == features::kGlicChromeStatusIconOtherAppID.Get()) {
    base::UmaHistogramBoolean("Glic.SetAltOSIcon.OnOtherAppStart",
                              !prefs_->GetBoolean(prefs::kGlicUseAltOSIcon));
    if (!features::kGlicChromeStatusIconLogOnly.Get()) {
      SetUseAltIcon(true);
    }
  }
}

}  // namespace glic
