// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/save_package_file_picker.h"

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/i18n/file_util_icu.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"

using content::RenderProcessHost;
using content::SavePageType;
using content::WebContents;

namespace {

// If false, we don't prompt the user as to where to save the file.  This
// exists only for testing.
bool g_should_prompt_for_filename = true;

void OnSavePackageDownloadCreated(download::DownloadItem* download) {
  ChromeDownloadManagerDelegate::DisableSafeBrowsing(download);
}

// Adds "Webpage, HTML Only" type to FileTypeInfo.
void AddHtmlOnlyFileTypeInfo(
    ui::SelectFileDialog::FileTypeInfo* file_type_info,
    const base::FilePath::StringType& extra_extension) {
  file_type_info->extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_SAVE_PAGE_DESC_HTML_ONLY));

  std::vector<base::FilePath::StringType> extensions;
  extensions.push_back(FILE_PATH_LITERAL("html"));
  extensions.push_back(FILE_PATH_LITERAL("htm"));
  if (!extra_extension.empty())
    extensions.push_back(extra_extension);
  file_type_info->extensions.emplace_back(std::move(extensions));
}

// Adds "Webpage, Single File" type to FileTypeInfo.
void AddSingleFileFileTypeInfo(
    ui::SelectFileDialog::FileTypeInfo* file_type_info) {
  file_type_info->extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_SAVE_PAGE_DESC_SINGLE_FILE));

  file_type_info->extensions.emplace_back(
      std::initializer_list<base::FilePath::StringType>{
          FILE_PATH_LITERAL("mhtml")});
}

// Chrome OS doesn't support HTML-Complete. crbug.com/154823
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Adds "Webpage, Complete" type to FileTypeInfo.
void AddCompleteFileTypeInfo(
    ui::SelectFileDialog::FileTypeInfo* file_type_info,
    const base::FilePath::StringType& extra_extension) {
  file_type_info->extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_SAVE_PAGE_DESC_COMPLETE));

  std::vector<base::FilePath::StringType> extensions;
  extensions.push_back(FILE_PATH_LITERAL("htm"));
  extensions.push_back(FILE_PATH_LITERAL("html"));
  if (!extra_extension.empty())
    extensions.push_back(extra_extension);
  file_type_info->extensions.push_back(extensions);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Checks whether this is a blocked page (e.g., when a child user is accessing
// a mature site).
// Recall that the blocked page is an interstitial. In the past, old
// (non-committed) interstitials couldn't be easily identified, while the
// committed ones can only be matched by page title. To prevent future bugs due
// to changing the page title, we make a conservative choice here and only
// check for PAGE_TYPE_ERROR. The result is that we may include a few other
// error pages (failed DNS lookups, SSL errors, etc), which shouldn't affect
// functionality.
bool IsErrorPage(content::WebContents* web_contents) {
  if (web_contents->GetController().GetActiveEntry() == nullptr)
    return false;
  return web_contents->GetController().GetLastCommittedEntry()->GetPageType() ==
         content::PAGE_TYPE_ERROR;
}

}  // anonymous namespace

// TODO(crbug.com/41439108): REMOVE DIRTY HACK
// To prevent access to blocked websites, we are temporarily disabling the
// HTML-only download of error pages for child users only.
// Note that MHTML is still available, so the save functionality is preserved.
bool SavePackageFilePicker::ShouldSaveAsOnlyHTML(
    content::WebContents* web_contents) const {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return !profile->IsChild() || !IsErrorPage(web_contents);
}

bool SavePackageFilePicker::ShouldSaveAsMHTML() const {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSavePageAsMHTML))
    return false;
#endif
  return can_save_as_complete_;
}

SavePackageFilePicker::SavePackageFilePicker(
    content::WebContents* web_contents,
    const base::FilePath& suggested_path,
    const base::FilePath::StringType& default_extension,
    bool can_save_as_complete,
    DownloadPrefs* download_prefs,
    content::SavePackagePathPickedCallback callback)
    : render_process_id_(
          web_contents->GetPrimaryMainFrame()->GetProcess()->GetID()),
      can_save_as_complete_(can_save_as_complete),
      download_prefs_(download_prefs),
      callback_(std::move(callback)) {
  base::FilePath suggested_path_copy = suggested_path;
  base::FilePath::StringType default_extension_copy = default_extension;
  size_t file_type_index = 0;
  ui::SelectFileDialog::FileTypeInfo file_type_info;

  file_type_info.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;

  if (can_save_as_complete_) {
    // The option index is not zero-based. Put a dummy entry.
    save_types_.push_back(content::SAVE_PAGE_TYPE_UNKNOWN);

    base::FilePath::StringType extra_extension;
    if (!ShouldSaveAsMHTML() && !suggested_path_copy.FinalExtension().empty() &&
        !suggested_path_copy.MatchesExtension(FILE_PATH_LITERAL(".htm")) &&
        !suggested_path_copy.MatchesExtension(FILE_PATH_LITERAL(".html"))) {
      extra_extension = suggested_path_copy.FinalExtension().substr(1);
    }

    if (ShouldSaveAsOnlyHTML(web_contents)) {
      AddHtmlOnlyFileTypeInfo(&file_type_info, extra_extension);
      save_types_.push_back(content::SAVE_PAGE_TYPE_AS_ONLY_HTML);
    }

    if (can_save_as_complete_) {
      AddSingleFileFileTypeInfo(&file_type_info);
      save_types_.push_back(content::SAVE_PAGE_TYPE_AS_MHTML);
    }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    AddCompleteFileTypeInfo(&file_type_info, extra_extension);
    save_types_.push_back(content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    file_type_info.include_all_files = false;

    content::SavePageType preferred_save_type =
        static_cast<content::SavePageType>(download_prefs_->save_file_type());
    if (ShouldSaveAsMHTML())
      preferred_save_type = content::SAVE_PAGE_TYPE_AS_MHTML;

    // Select the item saved in the pref.
    for (size_t i = 0; i < save_types_.size(); ++i) {
      if (save_types_[i] == preferred_save_type) {
        file_type_index = i;
        break;
      }
    }

    // If the item saved in the pref was not found, use the last item.
    if (!file_type_index)
      file_type_index = save_types_.size() - 1;
  } else {
    // The contents can not be saved as complete-HTML, so do not show the file
    // filters.
    file_type_info.extensions.resize(1);
    file_type_info.extensions[0].push_back(
        suggested_path_copy.FinalExtension());

    if (!file_type_info.extensions[0][0].empty()) {
      // Drop the .
      file_type_info.extensions[0][0].erase(0, 1);
    }

    file_type_info.include_all_files = true;
    file_type_index = 1;
  }

  if (file_type_index < save_types_.size() &&
      save_types_[file_type_index] == content::SAVE_PAGE_TYPE_AS_MHTML) {
    default_extension_copy = FILE_PATH_LITERAL("mhtml");
    suggested_path_copy =
        suggested_path_copy.ReplaceExtension(default_extension_copy);
  }

  if (g_should_prompt_for_filename) {
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
    select_file_dialog_->SelectFile(
        ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(),
        suggested_path_copy, &file_type_info, file_type_index,
        default_extension_copy,
        platform_util::GetTopLevel(web_contents->GetNativeView()),
        /*caller=*/
        web_contents
            ? &web_contents->GetPrimaryMainFrame()->GetLastCommittedURL()
            : nullptr);
    return;
  }

  // If |g_should_prompt_for_filename| is unset or |select_file_dialog_| could
  // not be instantiated for some reason, just use 'suggested_path_copy' instead
  // of opening the dialog prompt. Go through FileSelected() for consistency.
  FileSelected(ui::SelectedFileInfo(suggested_path_copy), file_type_index);
}

SavePackageFilePicker::~SavePackageFilePicker() {
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void SavePackageFilePicker::SetShouldPromptUser(bool should_prompt) {
  g_should_prompt_for_filename = should_prompt;
}

void SavePackageFilePicker::FileSelected(const ui::SelectedFileInfo& file,
                                         int index) {
  std::unique_ptr<SavePackageFilePicker> delete_this(this);
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;
  SavePageType save_type = content::SAVE_PAGE_TYPE_UNKNOWN;

  if (can_save_as_complete_) {
    DCHECK_LT(index, static_cast<int>(save_types_.size()));
    save_type = save_types_[index];
    if (select_file_dialog_ &&
        select_file_dialog_->HasMultipleFileTypeChoices()) {
      download_prefs_->SetSaveFileType(save_type);
    }
  } else {
    // Use "HTML Only" type as a dummy.
    save_type = content::SAVE_PAGE_TYPE_AS_ONLY_HTML;
  }

  base::FilePath path = file.path();
  base::i18n::NormalizeFileNameEncoding(&path);

  download_prefs_->SetSaveFilePath(path.DirName());

  content::SavePackagePathPickedParams params;
  params.file_path = path;
  params.save_type = save_type;
#if BUILDFLAG(IS_MAC)
  params.file_tags = file.file_tags;
#endif
  std::move(callback_).Run(std::move(params),
                           base::BindOnce(&OnSavePackageDownloadCreated));
}

void SavePackageFilePicker::FileSelectionCanceled() {
  delete this;
}
