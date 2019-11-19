// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_dialog_cloud.h"

#include <stddef.h>
#include <stdint.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/chrome_switches.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/render_frame_host.h"

namespace print_dialog_cloud {

namespace {

// Limit is used only to allocate memory. Cloud has own small limit.
const int kMaxFileSize = 1024 * 1024 * 1024;

// Sets print data into Cloud Print widget.
class PrintDataSetter : public content::WebContentsObserver {
 public:
  PrintDataSetter(content::WebContents* web_contents,
                  scoped_refptr<base::RefCountedMemory> data,
                  const base::string16& print_job_title,
                  const base::string16& print_ticket,
                  const std::string& file_type)
      : WebContentsObserver(web_contents) {
    DCHECK_NE(data->size(), 0u);
    std::string base64_data;
    base::Base64Encode(base::StringPiece(data->front_as<char>(), data->size()),
                       &base64_data);
    std::string header("data:");
    header.append(file_type);
    header.append(";base64,");
    base64_data.insert(0, header);

    base::DictionaryValue message_data;
    message_data.SetString("type", "dataUrl");
    message_data.SetString("title", print_job_title);
    message_data.SetString("content", base64_data);
    std::string json_data;
    base::JSONWriter::Write(message_data, &json_data);
    message_data_ = L"cp-dialog-set-print-document::";
    message_data_.append(base::UTF8ToUTF16(json_data));
  }

 private:
  // Overridden from content::WebContentsObserver:
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override {
    GURL url = web_contents()->GetURL();
    if (cloud_devices::IsCloudPrintURL(url)) {
      base::string16 origin = base::UTF8ToUTF16(url.GetOrigin().spec());
      content::MessagePortProvider::PostMessageToFrame(
          web_contents(), origin, origin, message_data_);
    }
  }

  void WebContentsDestroyed() override { delete this; }

  base::string16 message_data_;
  DISALLOW_COPY_AND_ASSIGN(PrintDataSetter);
};

void CreatePrintDialog(content::BrowserContext* browser_context,
                       const base::string16& print_job_title,
                       const base::string16& print_ticket,
                       const std::string& file_type,
                       scoped_refptr<base::RefCountedMemory> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  GURL url = cloud_devices::GetCloudPrintRelativeURL("client/dialog.html");
  content::WebContents* web_contents =
      displayer.browser()->OpenURL(content::OpenURLParams(
          google_util::AppendGoogleLocaleParam(
              url, g_browser_process->GetApplicationLocale()),
          content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_AUTO_BOOKMARK, false));
  if (data && data->size()) {
    new PrintDataSetter(web_contents, data, print_job_title, print_ticket,
                        file_type);
  }
}

scoped_refptr<base::RefCountedMemory> ReadFile(
    const base::FilePath& path_to_file) {
  scoped_refptr<base::RefCountedMemory> data;
  int64_t file_size = 0;
  if (base::GetFileSize(path_to_file, &file_size) && file_size != 0) {
    if (file_size > kMaxFileSize) {
      DLOG(WARNING) << " print data file too large to reserve space";
      return data;
    }
    std::string file_data;
    file_data.reserve(static_cast<size_t>(file_size));
    if (base::ReadFileToString(path_to_file, &file_data))
      data = base::RefCountedString::TakeString(&file_data);
  }
  base::DeleteFile(path_to_file, false);
  return data;
}

void CreatePrintDialogForFile(content::BrowserContext* browser_context,
                              const base::FilePath& path_to_file,
                              const base::string16& print_job_title,
                              const base::string16& print_ticket,
                              const std::string& file_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadFile, path_to_file),
      base::BindOnce(&CreatePrintDialog, browser_context, print_job_title,
                     print_ticket, file_type));
}

}  // namespace

bool CreatePrintDialogFromCommandLine(Profile* profile,
                                      const base::CommandLine& command_line) {
  base::FilePath cloud_print_file =
      command_line.GetSwitchValuePath(switches::kCloudPrintFile);
  DCHECK(!cloud_print_file.empty());
  if (cloud_print_file.empty())
    return false;
  base::string16 print_job_title =
      command_line.GetSwitchValueNative(switches::kCloudPrintJobTitle);
  base::string16 print_job_print_ticket =
      command_line.GetSwitchValueNative(switches::kCloudPrintPrintTicket);
  std::string file_type =
      command_line.GetSwitchValueASCII(switches::kCloudPrintFileType);
  if (file_type.empty())
    file_type = "application/pdf";
  CreatePrintDialogForFile(profile, cloud_print_file, print_job_title,
                           print_job_print_ticket, file_type);
  return true;
}

}  // namespace print_dialog_cloud
