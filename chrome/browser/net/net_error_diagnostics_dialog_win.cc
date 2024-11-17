// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_diagnostics_dialog.h"

#include <windows.h>
#include <winsock2.h>

#include <ndfapi.h>

#include <memory>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/native_library.h"
#include "base/scoped_native_library.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/threading/thread.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/base_shell_dialog_win.h"
#include "ui/views/win/hwnd_util.h"

namespace {

class NetErrorDiagnosticsDialog : public ui::BaseShellDialogImpl {
 public:
  NetErrorDiagnosticsDialog() {}

  NetErrorDiagnosticsDialog(const NetErrorDiagnosticsDialog&) = delete;
  NetErrorDiagnosticsDialog& operator=(const NetErrorDiagnosticsDialog&) =
      delete;

  ~NetErrorDiagnosticsDialog() override {}

  // NetErrorDiagnosticsDialog implementation.
  void Show(content::WebContents* web_contents,
            const std::string& failed_url,
            base::OnceClosure callback) {
    DCHECK(!callback.is_null());

    HWND parent =
        views::HWNDForNativeWindow(web_contents->GetTopLevelNativeWindow());
    if (IsRunningDialogForOwner(parent))
      return;

    std::unique_ptr<RunState> run_state = BeginRun(parent);

    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        run_state->dialog_task_runner;
    task_runner->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&NetErrorDiagnosticsDialog::ShowDialogOnPrivateThread,
                       base::Unretained(this), parent, failed_url),
        base::BindOnce(&NetErrorDiagnosticsDialog::DiagnosticsDone,
                       base::Unretained(this), std::move(run_state),
                       std::move(callback)));
  }

 private:
// TODO(crbug.com/370065739): The Ndf* functions here have been deprecated.
// Update this function and then remove these pragmas.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  void ShowDialogOnPrivateThread(HWND parent, const std::string& failed_url) {
    NDFHANDLE incident_handle;
    std::wstring failed_url_wide = base::UTF8ToWide(failed_url);
    if (!SUCCEEDED(NdfCreateWebIncident(failed_url_wide.c_str(),
                                        &incident_handle))) {
      return;
    }
    NdfExecuteDiagnosis(incident_handle, parent);
    NdfCloseIncident(incident_handle);
  }
#pragma clang diagnostic pop

  void DiagnosticsDone(std::unique_ptr<RunState> run_state,
                       base::OnceClosure callback) {
    EndRun(std::move(run_state));
    std::move(callback).Run();
  }
};

}  // namespace

bool CanShowNetworkDiagnosticsDialog(content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // The Windows diagnostic tool logs URLs it's run with, so it shouldn't be
  // used with incognito or guest profiles.  See https://crbug.com/929141
  return !profile->IsIncognitoProfile() && !profile->IsGuestSession();
}

void ShowNetworkDiagnosticsDialog(content::WebContents* web_contents,
                                  const std::string& failed_url) {
  DCHECK(CanShowNetworkDiagnosticsDialog(web_contents));

  NetErrorDiagnosticsDialog* dialog = new NetErrorDiagnosticsDialog();
  dialog->Show(
      web_contents, failed_url,
      base::BindOnce(&base::DeletePointer<NetErrorDiagnosticsDialog>, dialog));
}
