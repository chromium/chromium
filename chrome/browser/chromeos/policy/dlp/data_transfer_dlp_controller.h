// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DATA_TRANSFER_DLP_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DATA_TRANSFER_DLP_CONTROLLER_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_drag_drop_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/dragdrop/os_exchange_data.h"

namespace ui {
class DataTransferEndpoint;
}

namespace policy {

// DataTransferDlpController is responsible for preventing leaks of confidential
// data through clipboard data read or drag-and-drop by controlling read
// operations according to the rules of the Data leak prevention policy set by
// the admin.
class DataTransferDlpController : public ui::DataTransferPolicyController {
 public:
  // Creates an instance of the class.
  // Indicates that restricting clipboard content and drag-n-drop is required.
  // It's guaranteed that `dlp_rules_manager` controls the lifetime of
  // DataTransferDlpController and outlives it.
  static void Init(const DlpRulesManager& dlp_rules_manager);

  DataTransferDlpController(const DataTransferDlpController&) = delete;
  void operator=(const DataTransferDlpController&) = delete;

  // ui::DataTransferPolicyController:
  bool IsClipboardReadAllowed(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      const std::optional<size_t> size) override;
  void PasteIfAllowed(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      absl::variant<size_t, std::vector<base::FilePath>> pasted_content,
      content::RenderFrameHost* rfh,
      base::OnceCallback<void(bool)> paste_cb) override;
  void DropIfAllowed(std::optional<ui::DataTransferEndpoint> data_src,
                     std::optional<ui::DataTransferEndpoint> data_dst,
                     std::optional<std::vector<ui::FileInfo>> filenames,
                     base::OnceClosure drop_cb) override;

 protected:
  explicit DataTransferDlpController(const DlpRulesManager& dlp_rules_manager);
  ~DataTransferDlpController() override;

  // Returns maximal time for which reporting can be skipped.
  // See LastReportedEndpoints for details.
  // Should be overridden in tests (increased).
  virtual base::TimeDelta GetSkipReportingTimeout();

  // Protected because it needs to be accessible from tests.
  void ReportWarningProceededEvent(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      const std::string& src_pattern,
      const std::string& dst_pattern,
      bool is_clipboard_event,
      const DlpRulesManager::RuleMetadata& rule_metadata);

 private:
  virtual void NotifyBlockedPaste(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst);

  virtual void WarnOnPaste(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      base::OnceClosure reporting_cb);

  virtual void WarnOnBlinkPaste(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> paste_cb);

  virtual bool ShouldPasteOnWarn(
      base::optional_ref<const ui::DataTransferEndpoint> data_dst);

  virtual bool ShouldCancelOnWarn(
      base::optional_ref<const ui::DataTransferEndpoint> data_dst);

  virtual void NotifyBlockedDrop(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst);

  virtual void WarnOnDrop(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      base::OnceClosure drop_cb);

  bool ShouldSkipReporting(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      bool is_warning_proceeded,
      base::TimeTicks curr_time);

  void ReportEvent(base::optional_ref<const ui::DataTransferEndpoint> data_src,
                   base::optional_ref<const ui::DataTransferEndpoint> data_dst,
                   const std::string& src_pattern,
                   const std::string& dst_pattern,
                   DlpRulesManager::Level level,
                   bool is_clipboard_event,
                   const DlpRulesManager::RuleMetadata& rule_metadata);

  void MaybeReportEvent(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      const std::string& src_pattern,
      const std::string& dst_pattern,
      DlpRulesManager::Level level,
      bool is_clipboard_event,
      const DlpRulesManager::RuleMetadata& rule_metadata);

  void ContinueDropIfAllowed(std::optional<ui::DataTransferEndpoint> data_src,
                             std::optional<ui::DataTransferEndpoint> data_dst,
                             base::OnceClosure drop_cb);

  // Performs clipbpoard restriction related checks.
  void ContinuePasteIfClipboardRestrictionsAllow(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      size_t size,
      content::RenderFrameHost* rfh,
      base::OnceCallback<void(bool)> paste_cb);

  // The solution for the issue of sending multiple reporting events for a
  // single user action. When a user triggers a paste (for instance by pressing
  // ctrl+V) clipboard API receives multiple mojo calls. For each call we check
  // if restricted data is being accessed. However, there is no way to identify
  // if those API calls come from the same user action or not. So after
  // reporting one event, we skip reporting for a short time.
  struct LastReportedEndpoints {
    LastReportedEndpoints();
    ~LastReportedEndpoints();
    std::optional<ui::DataTransferEndpoint> data_src;
    std::optional<ui::DataTransferEndpoint> data_dst;
    std::optional<bool> is_warning_proceeded;
    base::TimeTicks time;
  } last_reported_;

  const raw_ref<const DlpRulesManager> dlp_rules_manager_;
  DlpClipboardNotifier clipboard_notifier_;
  DlpDragDropNotifier drag_drop_notifier_;

  base::WeakPtrFactory<DataTransferDlpController> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DATA_TRANSFER_DLP_CONTROLLER_H_
