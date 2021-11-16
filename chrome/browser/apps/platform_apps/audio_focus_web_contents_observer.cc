// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/audio_focus_web_contents_observer.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"

namespace apps {

AudioFocusWebContentsObserver::AudioFocusWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<AudioFocusWebContentsObserver>(
          *web_contents) {}

AudioFocusWebContentsObserver::~AudioFocusWebContentsObserver() = default;

void AudioFocusWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() || navigation_handle->IsErrorPage()) {
    return;
  }

  if (!audio_focus_group_id_.is_empty())
    return;

  content::BrowserContext* context =
      navigation_handle->GetWebContents()->GetBrowserContext();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(context)
          ->enabled_extensions()
          .GetExtensionOrAppByURL(navigation_handle->GetURL());

  if (!extension || !extension->is_platform_app())
    return;

  extensions::ProcessManager* pm = extensions::ProcessManager::Get(context);
  extensions::ExtensionHost* host =
      pm->GetBackgroundHostForExtension(extension->id());

  if (!host || !host->host_contents())
    return;

  // If we are the background contents then we will generate a group id.
  // Otherwise, we will use the group id from the background contents. This
  // means that all web contents for an app will have the same group id. This
  // will only be done once for a given web contents and expects it to not
  // change platform app during its lifetime.
  if (host->host_contents() == web_contents()) {
    audio_focus_group_id_ = base::UnguessableToken::Create();
  } else {
    AudioFocusWebContentsObserver* observer =
        AudioFocusWebContentsObserver::FromWebContents(host->host_contents());
    DCHECK(observer);

    audio_focus_group_id_ = observer->audio_focus_group_id_;
  }

  content::MediaSession::Get(web_contents())
      ->SetAudioFocusGroupId(audio_focus_group_id_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AudioFocusWebContentsObserver);

}  // namespace apps
