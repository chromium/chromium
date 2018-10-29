// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_profile_observer.h"

#include "base/callback.h"
#include "base/task/post_task.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feedback/feedback_report.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

static base::LazyInstance<feedback::FeedbackProfileObserver>::Leaky
    g_feedback_profile_observer = LAZY_INSTANCE_INITIALIZER;

namespace feedback {

// static
void FeedbackProfileObserver::Initialize() {
  g_feedback_profile_observer.Get();
}

FeedbackProfileObserver::FeedbackProfileObserver() {
  prefs_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                       content::NotificationService::AllSources());
}

FeedbackProfileObserver::~FeedbackProfileObserver() {
  prefs_registrar_.RemoveAll();
}

void FeedbackProfileObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_CREATED, type);

  Profile* profile = content::Source<Profile>(source).ptr();
  if (profile && !profile->IsOffTheRecord())
    QueueUnsentReports(profile);
}

void FeedbackProfileObserver::QueueSingleReport(
    feedback::FeedbackUploader* uploader,
    std::unique_ptr<std::string> data) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&FeedbackUploaderChrome::QueueReport,
                     uploader->AsWeakPtr(), std::move(data)));
}

void FeedbackProfileObserver::QueueUnsentReports(
    content::BrowserContext* context) {
  feedback::FeedbackUploaderChrome* uploader =
      feedback::FeedbackUploaderFactoryChrome::GetForBrowserContext(context);
  uploader->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FeedbackReport::LoadReportsAndQueue,
                                uploader->feedback_reports_path(),
                                base::BindRepeating(
                                    &FeedbackProfileObserver::QueueSingleReport,
                                    uploader)));
}

}  // namespace feedback
