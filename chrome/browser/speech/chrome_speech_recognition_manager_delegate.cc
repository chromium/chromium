// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"

#include <string>

#include "base/functional/bind.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_error.mojom.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_result.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"
#endif

using content::BrowserThread;
using content::SpeechRecognitionManager;
using content::WebContents;

namespace speech {

ChromeSpeechRecognitionManagerDelegate
::ChromeSpeechRecognitionManagerDelegate() {
}

ChromeSpeechRecognitionManagerDelegate
::~ChromeSpeechRecognitionManagerDelegate() {
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionStart(
    int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnAudioStart(int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnEnvironmentEstimationComplete(
    int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnSoundStart(int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnSoundEnd(int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnAudioEnd(int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionResults(
    int session_id,
    const std::vector<blink::mojom::SpeechRecognitionResultPtr>& result) {}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionError(
    int session_id,
    const blink::mojom::SpeechRecognitionError& error) {}

void ChromeSpeechRecognitionManagerDelegate::OnAudioLevelsChange(
    int session_id, float volume, float noise_volume) {
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionEnd(int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::CheckRecognitionIsAllowed(
    int session_id,
    base::OnceCallback<void(bool ask_user, bool is_allowed)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const content::SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);

  // Make sure that initiators (extensions/web pages) properly set the
  // |render_process_id| field, which is needed later to retrieve the profile.
  DCHECK_NE(context.render_process_id, 0);

  int render_process_id = context.render_process_id;
  int render_frame_id = context.render_frame_id;
  if (context.embedder_render_process_id) {
    // If this is a request originated from a guest, we need to re-route the
    // permission check through the embedder (app).
    render_process_id = context.embedder_render_process_id;
    render_frame_id = context.embedder_render_frame_id;
  }

  // Check that the render frame type is appropriate, and whether or not we
  // need to request permission from the user.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CheckRenderFrameType, std::move(callback),
                                render_process_id, render_frame_id));
}

content::SpeechRecognitionEventListener*
ChromeSpeechRecognitionManagerDelegate::GetEventListener() {
  return this;
}

bool ChromeSpeechRecognitionManagerDelegate::FilterProfanities(
    int render_process_id) {
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(render_process_id);
  if (!rph)  // Guard against race conditions on RPH lifetime.
    return true;

  return Profile::FromBrowserContext(rph->GetBrowserContext())->GetPrefs()->
      GetBoolean(prefs::kSpeechRecognitionFilterProfanities);
}

// static.
void ChromeSpeechRecognitionManagerDelegate::CheckRenderFrameType(
    base::OnceCallback<void(bool ask_user, bool is_allowed)> callback,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);

  bool allowed = false;
  bool check_permission = false;

  if (!render_frame_host) {
    // This happens for extensions. Manifest should be checked for permission.
    allowed = true;
    check_permission = false;
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), check_permission, allowed));
    return;
  }

  if (render_frame_host->GetLifecycleState() ==
      content::RenderFrameHost::LifecycleState::kPrerendering) {
    // It's unclear whether we can reach this function during prerendering.
    // The Mojo binding for blink.mojom.SpeechRecognizer is deferred until
    // activation, but it's conceivable that callsites that do not originate
    // from SpeechRecognizer can call this method.
    allowed = false;
    check_permission = false;
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), check_permission, allowed));
    return;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  extensions::mojom::ViewType view_type = extensions::GetViewType(web_contents);

  if (view_type == extensions::mojom::ViewType::kTabContents ||
      view_type == extensions::mojom::ViewType::kAppWindow ||
      view_type == extensions::mojom::ViewType::kComponent ||
      view_type == extensions::mojom::ViewType::kExtensionPopup ||
      view_type == extensions::mojom::ViewType::kExtensionBackgroundPage ||
      view_type == extensions::mojom::ViewType::kExtensionSidePanel) {
    // If it is a tab, we can check for permission. For apps, this means
    // manifest would be checked for permission.
    allowed = true;
    check_permission = true;
  }
#else
  // Otherwise this should be a regular tab contents.
  allowed = true;
  check_permission = true;
#endif

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), check_permission, allowed));
}

}  // namespace speech
