// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SELECT_HELPER_H_
#define CHROME_BROWSER_FILE_SELECT_HELPER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/files_scan_data.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/directory_lister.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#endif

class Profile;
class ScopedDisallowPictureInPicture;

namespace content {
class FileSelectListener;
class WebContents;
}

namespace ui {
struct SelectedFileInfo;
}

namespace policy {
FORWARD_DECLARE_TEST(DlpFilesControllerAshBrowserTest, FilesUploadCallerPassed);
}  // namespace policy

// This class handles file-selection requests coming from renderer processes.
// It implements both the initialisation and listener functions for
// file-selection dialogs.
//
// Since FileSelectHelper listens to WebContentsObserver, it needs to live on
// and be destroyed on the UI thread. References to FileSelectHelper may be
// passed on to other threads.
class FileSelectHelper : public base::RefCountedThreadSafe<
                             FileSelectHelper,
                             content::BrowserThread::DeleteOnUIThread>,
                         public ui::SelectFileDialog::Listener,
                         public content::WebContentsObserver,
                         private net::DirectoryLister::DirectoryListerDelegate {
 public:
  FileSelectHelper(const FileSelectHelper&) = delete;
  FileSelectHelper& operator=(const FileSelectHelper&) = delete;

  // Show the file chooser dialog.
  static void RunFileChooser(
      content::RenderFrameHost* render_frame_host,
      scoped_refptr<content::FileSelectListener> listener,
      const blink::mojom::FileChooserParams& params);

  // Enumerates all the files in directory.
  static void EnumerateDirectory(
      content::WebContents* tab,
      scoped_refptr<content::FileSelectListener> listener,
      const base::FilePath& path);

 private:
  friend class base::RefCountedThreadSafe<FileSelectHelper>;
  friend class base::DeleteHelper<FileSelectHelper>;
  friend class FileSelectHelperContactsAndroid;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;

  FRIEND_TEST_ALL_PREFIXES(FileSelectHelperTest, IsAcceptTypeValid);
  FRIEND_TEST_ALL_PREFIXES(FileSelectHelperTest, ZipPackage);
  FRIEND_TEST_ALL_PREFIXES(FileSelectHelperTest, GetSanitizedFileName);
  FRIEND_TEST_ALL_PREFIXES(FileSelectHelperTest, LastSelectedDirectory);
  FRIEND_TEST_ALL_PREFIXES(FileSelectHelperTest,
                           ContentAnalysisCompletionCallback_NoFiles);
  FRIEND_TEST_ALL_PREFIXES(FileSelectHelperTest,
                           ContentAnalysisCompletionCallback_OneOKFile);
  FRIEND_TEST_ALL_PREFIXES(FileSelectHelperTest,
                           ContentAnalysisCompletionCallback_TwoOKFiles);
  FRIEND_TEST_ALL_PREFIXES(FileSelectHelperTest,
                           ContentAnalysisCompletionCallback_TwoBadFiles);
  FRIEND_TEST_ALL_PREFIXES(FileSelectHelperTest,
                           ContentAnalysisCompletionCallback_OKBadFiles);
  FRIEND_TEST_ALL_PREFIXES(
      FileSelectHelperTest,
      ContentAnalysisCompletionCallback_SystemFilesSkipped);
  FRIEND_TEST_ALL_PREFIXES(FileSelectHelperTest,
                           ContentAnalysisCompletionCallback_SystemOKBadFiles);
  FRIEND_TEST_ALL_PREFIXES(FileSelectHelperTest,
                           ContentAnalysisCompletionCallback_FolderUpload_OK);
  FRIEND_TEST_ALL_PREFIXES(FileSelectHelperTest,
                           ContentAnalysisCompletionCallback_FolderUpload_Bad);
  FRIEND_TEST_ALL_PREFIXES(
      FileSelectHelperTest,
      ContentAnalysisCompletionCallback_FolderUploadBlockedThenAllowed);
  FRIEND_TEST_ALL_PREFIXES(
      FileSelectHelperTest,
      ContentAnalysisCompletionCallback_FolderUpload_OKBad);
  FRIEND_TEST_ALL_PREFIXES(FileSelectHelperTest, GetFileTypesFromAcceptType);
  FRIEND_TEST_ALL_PREFIXES(FileSelectHelperTest, MultipleFileExtensionsForMime);
  FRIEND_TEST_ALL_PREFIXES(policy::DlpFilesControllerAshBrowserTest,
                           FilesUploadCallerPassed);

  explicit FileSelectHelper(Profile* profile);
  ~FileSelectHelper() override;

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      blink::mojom::FileChooserParamsPtr params);
  void GetFileTypesInThreadPool(blink::mojom::FileChooserParamsPtr params);
  void GetSanitizedFilenameOnUIThread(
      blink::mojom::FileChooserParamsPtr params);
#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Safe Browsing checks are only applied when `params->mode` is
  // `kSave`, which is only for PPAPI requests.
  void CheckDownloadRequestWithSafeBrowsing(
      const base::FilePath& default_path,
      blink::mojom::FileChooserParamsPtr params);
  void ProceedWithSafeBrowsingVerdict(const base::FilePath& default_path,
                                      blink::mojom::FileChooserParamsPtr params,
                                      bool allowed_by_safe_browsing);
#endif
  void RunFileChooserOnUIThread(const base::FilePath& default_path,
                                blink::mojom::FileChooserParamsPtr params);

  // Cleans up and releases this instance. This must be called after the last
  // callback is received from the file chooser dialog.
  void RunFileChooserEnd();

  // SelectFileDialog::Listener overrides.
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override;
  void FileSelectionCanceled() override;

  // content::WebContentsObserver overrides.
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  void EnumerateDirectoryImpl(
      content::WebContents* tab,
      scoped_refptr<content::FileSelectListener> listener,
      const base::FilePath& path);

  // Kicks off a new directory enumeration.
  void StartNewEnumeration(const base::FilePath& path);

  // net::DirectoryLister::DirectoryListerDelegate overrides.
  void OnListFile(
      const net::DirectoryLister::DirectoryListerData& data) override;
  void OnListDone(int error) override;

  void LaunchConfirmationDialog(
      const base::FilePath& path,
      std::vector<ui::SelectedFileInfo> selected_files);

  // Cleans up and releases this instance. This must be called after the last
  // callback is received from the enumeration code.
  void EnumerateDirectoryEnd();

#if BUILDFLAG(IS_MAC)
  // Must be called from a MayBlock() task. Each selected file that is a package
  // will be zipped, and the zip will be passed to the render view host in place
  // of the package.
  void ProcessSelectedFilesMac(const std::vector<ui::SelectedFileInfo>& files);

  // Saves the paths of |zipped_files| for later deletion. Passes |files| to the
  // render view host.
  void ProcessSelectedFilesMacOnUIThread(
      const std::vector<ui::SelectedFileInfo>& files,
      const std::vector<base::FilePath>& zipped_files);

  // Zips the package at |path| into a temporary destination. Returns the
  // temporary destination, if the zip was successful. Otherwise returns an
  // empty path.
  static base::FilePath ZipPackage(const base::FilePath& path);
#endif  // BUILDFLAG(IS_MAC)

  // This function is the start of a call chain that may or may not be async
  // depending on the platform and features enabled.  The call to this method
  // is made after the user has chosen the file(s) in the UI in order to
  // process and filter the list before returning the final result to the
  // caller.  The call chain is as follows:
  //
  // ConvertToFileChooserFileInfoList: converts a vector of SelectedFileInfo
  // into a vector of FileChooserFileInfoPtr and then calls
  // PerformContentAnalysisIfNeeded().  On chromeos, the conversion is
  // performed asynchronously.
  //
  // PerformContentAnalysisIfNeeded: if the deep scanning feature is
  // enabled and it is determined by enterprise policy that scans are required,
  // starts the scans and sets ContentAnalysisCompletionCallback() as the async
  // callback.  If deep scanning is not enabled or is not supported on the
  // platform, this function calls NotifyListenerAndEnd() directly.
  //
  // ContentAnalysisCompletionCallback: processes the results of the deep scan.
  // For folder upload, any files not passing the scan result in the entire
  // folder being blocked (the list cleared). For multiple-file upload, any
  // files that did not pass the scan are removed from the list. Ends by calling
  // NotifyListenerAndEnd().
  //
  // NotifyListenerAndEnd: Informs the listener of the final list of files to
  // use and performs any required cleanup.
  //
  // Because the state of the web contents may change at each asynchronous
  // step, calls are make to AbortIfWebContentsDestroyed() to check if, for
  // example, the tab has been closed or the contents navigated.  In these
  // cases the file selection is aborted and the state cleaned up.
  void ConvertToFileChooserFileInfoList(
      const std::vector<ui::SelectedFileInfo>& files);

  // Checks to see if scans are required for the specified files.
  void PerformContentAnalysisIfNeeded(
      std::vector<blink::mojom::FileChooserFileInfoPtr> list);

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  // Callback used to receive the results of a content analysis scan.
  void ContentAnalysisCompletionCallback(
      std::vector<blink::mojom::FileChooserFileInfoPtr> list,
      const enterprise_connectors::ContentAnalysisDelegate::Data& data,
      enterprise_connectors::ContentAnalysisDelegate::Result& result);
#endif

  // Finish the PerformContentAnalysisIfNeeded() handling after the
  // deep scanning checks have been performed.  Deep scanning may change the
  // list of files chosen by the user, so the list of files passed here may be
  // a subset of of the files passed to PerformContentAnalysisIfNeeded().
  void NotifyListenerAndEnd(
      std::vector<blink::mojom::FileChooserFileInfoPtr> list);

  // Schedules the deletion of the files in |temporary_files_| and clears the
  // vector.
  void DeleteTemporaryFiles();

  // Cleans up when the initiator of the file chooser is no longer valid.
  void CleanUp();

  // Calls RunFileChooserEnd() if the webcontents was destroyed. Returns true
  // if the file chooser operation shouldn't proceed.
  bool AbortIfWebContentsDestroyed();

  void SetFileSelectListenerForTesting(
      scoped_refptr<content::FileSelectListener> listener);

  void DontAbortOnMissingWebContentsForTesting();

  // Helper method to get allowed extensions for select file dialog from
  // the specified accept types as defined in the spec:
  //   http://whatwg.org/html/number-state.html#attr-input-accept
  // |accept_types| contains only valid lowercased MIME types or file extensions
  // beginning with a period (.).
  static std::unique_ptr<ui::SelectFileDialog::FileTypeInfo>
  GetFileTypesFromAcceptType(const std::vector<std::u16string>& accept_types);

  // Check the accept type is valid. It is expected to be all lower case with
  // no whitespace.
  static bool IsAcceptTypeValid(const std::string& accept_type);

  // Get a sanitized filename suitable for use as a default filename. The
  // suggested filename coming over the IPC may contain invalid characters or
  // may result in a filename that's reserved on the current platform.
  //
  // If |suggested_path| is empty, the return value is also empty.
  //
  // If |suggested_path| is non-empty, but can't be safely converted to UTF-8,
  // or is entirely lost during the sanitization process (e.g. because it
  // consists entirely of invalid characters), it's replaced with a default
  // filename.
  //
  // Otherwise, returns |suggested_path| with any invalid characters will be
  // replaced with a suitable replacement character.
  static base::FilePath GetSanitizedFileName(
      const base::FilePath& suggested_path);

  // Profile used to set/retrieve the last used directory.
  raw_ptr<Profile> profile_;

  // The RenderFrameHost and WebContents for the page showing a file dialog
  // (may only be one such dialog).
  raw_ptr<content::RenderFrameHost> render_frame_host_;
  raw_ptr<content::WebContents> web_contents_;

  // |listener_| receives the result of the FileSelectHelper.
  scoped_refptr<content::FileSelectListener> listener_;

  // Dialog box used for choosing files to upload from file form fields.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  std::unique_ptr<ui::SelectFileDialog::FileTypeInfo> select_file_types_;

  // The type of file dialog last shown. This is SELECT_NONE if an
  // instance is created through the public EnumerateDirectory().
  ui::SelectFileDialog::Type dialog_type_;

  // The mode of file dialog last shown.
  blink::mojom::FileChooserParams::Mode dialog_mode_;

  // The enumeration root directory for EnumerateDirectory() and
  // RunFileChooser with kUploadFolder.
  base::FilePath base_dir_;

  // Maintain an active directory enumeration.  These could come from the file
  // select dialog or from drag-and-drop of directories.  There could not be
  // more than one going on at a time.
  struct ActiveDirectoryEnumeration;
  std::unique_ptr<ActiveDirectoryEnumeration> directory_enumeration_;

  // Temporary files only used on OSX. This class is responsible for deleting
  // these files when they are no longer needed.
  std::vector<base::FilePath> temporary_files_;

  // Set to false in unit tests since there is no WebContents.
  bool abort_on_missing_web_contents_in_tests_ = true;

#if !BUILDFLAG(IS_ANDROID)
  // When not null, this prevents picture-in-picture windows from opening.
  std::unique_ptr<ScopedDisallowPictureInPicture>
      scoped_disallow_picture_in_picture_;
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::WeakPtrFactory<FileSelectHelper> weak_ptr_factory_{this};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

#endif  // CHROME_BROWSER_FILE_SELECT_HELPER_H_
