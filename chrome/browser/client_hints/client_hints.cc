// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <functional>
#include <string>

#include "chrome/browser/client_hints/client_hints.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/client_hints/client_hints.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/origin_util.h"

namespace client_hints {

ClientHints::ClientHints(content::BrowserContext* context)
    : context_(context) {}

ClientHints::ClientHints(content::WebContents* tab) {
  binding_ = std::make_unique<
      content::WebContentsFrameBindingSet<client_hints::mojom::ClientHints>>(
      tab, this);
}

content::BrowserContext* ClientHints::GetContext() {
  if (!context_) {
    content::RenderProcessHost* rph =
        binding_->GetCurrentTargetFrame()->GetProcess();
    DCHECK(rph);
    context_ = rph->GetBrowserContext();
  }
  return context_;
}

ClientHints::~ClientHints() = default;

network::NetworkQualityTracker* ClientHints::GetNetworkQualityTracker() {
  DCHECK(g_browser_process);
  return g_browser_process->network_quality_tracker();
}

void ClientHints::GetAllowedClientHintsFromSource(
    const GURL& url,
    blink::WebEnabledClientHints* client_hints) {
  ContentSettingsForOneType client_hints_rules;
  Profile* profile = Profile::FromBrowserContext(GetContext());
  if (!profile)
    return;

  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, std::string(), &client_hints_rules);
  client_hints::GetAllowedClientHintsFromSource(url, client_hints_rules,
                                                client_hints);
}

bool ClientHints::IsJavaScriptAllowed(const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(GetContext());
  if (!profile)
    return false;

  return HostContentSettingsMapFactory::GetForProfile(profile)
             ->GetContentSetting(url, url, ContentSettingsType::JAVASCRIPT,
                                 std::string()) == CONTENT_SETTING_ALLOW;
}

std::string ClientHints::GetAcceptLanguageString() {
  Profile* profile = Profile::FromBrowserContext(GetContext());
  if (!profile)
    return std::string();
  DCHECK(profile->GetPrefs());
  return profile->GetPrefs()->GetString(language::prefs::kAcceptLanguages);
}

blink::UserAgentMetadata ClientHints::GetUserAgentMetadata() {
  return ::GetUserAgentMetadata();
}

void ClientHints::PersistClientHints(
    const url::Origin& primary_origin,
    const std::vector<blink::mojom::WebClientHintsType>& client_hints,
    base::TimeDelta expiration_duration) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const GURL primary_url = primary_origin.GetURL();

  // TODO(tbansal): crbug.com/735518. Consider killing the renderer that sent
  // the malformed IPC.
  if (!primary_url.is_valid() || !content::IsOriginSecure(primary_url))
    return;

  DCHECK(!client_hints.empty());
  DCHECK_LE(
      client_hints.size(),
      static_cast<size_t>(blink::mojom::WebClientHintsType::kMaxValue) + 1);

  if (client_hints.empty() ||
      client_hints.size() >
          (static_cast<size_t>(blink::mojom::WebClientHintsType::kMaxValue) +
           1)) {
    // Return early if the list does not have the right number of values.
    // Persisting wrong number of values to the disk may cause errors when
    // reading them back in the future.
    return;
  }

  if (expiration_duration <= base::TimeDelta::FromSeconds(0))
    return;

  Profile* profile = Profile::FromBrowserContext(GetContext());

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  std::unique_ptr<base::ListValue> expiration_times_list =
      std::make_unique<base::ListValue>();
  expiration_times_list->Reserve(client_hints.size());

  // Use wall clock since the expiration time would be persisted across embedder
  // restarts.
  double expiration_time =
      (base::Time::Now() + expiration_duration).ToDoubleT();

  for (const auto& entry : client_hints)
    expiration_times_list->AppendInteger(static_cast<int>(entry));

  auto expiration_times_dictionary = std::make_unique<base::DictionaryValue>();
  expiration_times_dictionary->SetList("client_hints",
                                       std::move(expiration_times_list));
  expiration_times_dictionary->SetDouble("expiration_time", expiration_time);

  // TODO(tbansal): crbug.com/735518. Disable updates to client hints settings
  // when cookies are disabled for |primary_origin|.
  map->SetWebsiteSettingDefaultScope(
      primary_url, GURL(), ContentSettingsType::CLIENT_HINTS, std::string(),
      std::move(expiration_times_dictionary));

  UMA_HISTOGRAM_EXACT_LINEAR("ClientHints.UpdateEventCount", 1, 2);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ClientHints)

}  // namespace client_hints
