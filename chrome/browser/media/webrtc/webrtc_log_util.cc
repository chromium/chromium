// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_log_util.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/webrtc_logging/browser/log_cleanup.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "content/public/browser/browser_thread.h"

// static
void WebRtcLogUtil::DeleteOldWebRtcLogFilesForAllProfiles() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            &webrtc_logging::DeleteOldWebRtcLogFiles,
            webrtc_logging::TextLogList::
                GetWebRtcLogDirectoryForBrowserContextPath(entry->GetPath())));
  }
}
