// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/mock_report_sender.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace safe_browsing {

MockReportSender::MockReportSender()
    : net::ReportSender(nullptr, TRAFFIC_ANNOTATION_FOR_TESTS),
      number_of_reports_(0) {
  DCHECK(quit_closure_.is_null());
}

MockReportSender::~MockReportSender() {}

void MockReportSender::Send(
    const GURL& report_uri,
    base::StringPiece content_type,
    base::StringPiece report,
    const base::Callback<void()>& success_callback,
    const base::Callback<void(const GURL&, int, int)>& error_callback) {
  latest_report_uri_ = report_uri;
  report.CopyToString(&latest_report_);
  content_type.CopyToString(&latest_content_type_);
  number_of_reports_++;

  // BrowserThreads aren't initialized in the unittest, so don't post tasks
  // to them.
  if (!content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI))
    return;

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&MockReportSender::NotifyReportSentOnUIThread,
                                base::Unretained(this)));
}

void MockReportSender::WaitForReportSent() {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MockReportSender::NotifyReportSentOnUIThread() {
  if (!quit_closure_.is_null()) {
    quit_closure_.Run();
    quit_closure_.Reset();
  }
}

const GURL& MockReportSender::latest_report_uri() {
  return latest_report_uri_;
}

const std::string& MockReportSender::latest_report() {
  return latest_report_;
}

const std::string& MockReportSender::latest_content_type() {
  return latest_content_type_;
}

int MockReportSender::GetAndResetNumberOfReportsSent() {
  int new_reports = number_of_reports_;
  number_of_reports_ = 0;
  return new_reports;
}

}  // namespace safe_browsing
