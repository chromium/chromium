// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/origin_util.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

MediaStreamDevicePolicy GetDevicePolicy(const Profile* profile,
                                        const GURL& security_origin,
                                        const char* policy_name,
                                        const char* allowed_urls_pref_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the security origin policy matches a value in allowed urls list, allow
  // it.  Otherwise, check the |policy_name| switch for the default behavior.

  const PrefService* prefs = profile->GetPrefs();

  const base::Value::List& list = prefs->GetList(allowed_urls_pref_name);
  for (const base::Value& i : list) {
    const std::string* value = i.GetIfString();
    if (value) {
      ContentSettingsPattern pattern =
          ContentSettingsPattern::FromString(*value);
      if (pattern == ContentSettingsPattern::Wildcard()) {
        DLOG(WARNING) << "Ignoring wildcard URL pattern: " << *value;
        continue;
      }
      DLOG_IF(ERROR, !pattern.IsValid()) << "Invalid URL pattern: " << *value;
      if (pattern.IsValid() && pattern.Matches(security_origin))
        return ALWAYS_ALLOW;
    }
  }

  // If a match was not found, check if audio capture is otherwise disallowed
  // or if the user should be prompted.  Setting the policy value to "true"
  // is equal to not setting it at all, so from hereon out, we will return
  // either POLICY_NOT_SET (prompt) or ALWAYS_DENY (no prompt, no access).
  if (!prefs->GetBoolean(policy_name))
    return ALWAYS_DENY;

  return POLICY_NOT_SET;
}
