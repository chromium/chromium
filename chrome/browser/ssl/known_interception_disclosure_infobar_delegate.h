// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_SSL_KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE_H_

#include <algorithm>
#include "base/memory/singleton.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "net/cert/cert_status_flags.h"
#include "url/gurl.h"

namespace base {
class Clock;
}  // namespace base

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class Profile;

// Singleton that tracks the known interception disclosure cooldown time. On
// Android, this is measured across browser sessions (which tend to be short) by
// storing the last dismissal time in a pref. On Desktop, the last dismissal
// time is stored in memory, so this is is only measured within the same
// browsing session (and thus will trigger on every browser startup).
class KnownInterceptionDisclosureCooldown {
 public:
  static KnownInterceptionDisclosureCooldown* GetInstance();

  bool IsActive(Profile* profile);
  void Activate(Profile* profile);

  bool get_has_seen_known_interception() {
    return has_seen_known_interception_;
  }
  void set_has_seen_known_interception(bool has_seen) {
    has_seen_known_interception_ = has_seen;
  }

  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

 private:
  friend struct base::DefaultSingletonTraits<
      KnownInterceptionDisclosureCooldown>;

  KnownInterceptionDisclosureCooldown();
  ~KnownInterceptionDisclosureCooldown();

  std::unique_ptr<base::Clock> clock_ = std::make_unique<base::DefaultClock>();
  bool has_seen_known_interception_ = false;

#if !defined(OS_ANDROID)
  base::Time last_dismissal_time_;
#endif
};

// Shows the known interception disclosure UI if it has not been recently
// dismissed.
void MaybeShowKnownInterceptionDisclosureDialog(
    content::WebContents* web_contents,
    net::CertStatus cert_status);

class KnownInterceptionDisclosureInfoBarDelegate
    : public ConfirmInfoBarDelegate {
 public:
  explicit KnownInterceptionDisclosureInfoBarDelegate(Profile* profile);
  ~KnownInterceptionDisclosureInfoBarDelegate() override = default;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  bool Accept() override;

#if defined(OS_ANDROID)
  int GetIconId() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;

  // This function is the equivalent of GetMessageText(), but for the portion of
  // the infobar below the 'message' title for the Android infobar.
  base::string16 GetDescriptionText() const;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
#endif

 private:
  Profile* profile_;
};

#endif  // CHROME_BROWSER_SSL_KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE_H_
