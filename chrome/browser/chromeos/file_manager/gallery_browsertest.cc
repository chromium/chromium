// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"

namespace file_manager {

template <GuestMode MODE>
class GalleryBrowserTestBase : public FileManagerBrowserTestBase {
 public:
  GalleryBrowserTestBase() = default;

 protected:
  GuestMode GetGuestMode() const override { return MODE; }

  const char* GetTestCaseName() const override {
    return test_case_name_.c_str();
  }

  std::string GetFullTestCaseName() const override {
    return test_case_name_;
  }

  const char* GetTestExtensionManifestName() const override {
    return "gallery_test_manifest.json";
  }

  void set_test_case_name(const std::string& name) { test_case_name_ = name; }

 private:
  std::string test_case_name_;

  DISALLOW_COPY_AND_ASSIGN(GalleryBrowserTestBase);
};

typedef GalleryBrowserTestBase<NOT_IN_GUEST_MODE> GalleryBrowserTest;
typedef GalleryBrowserTestBase<IN_GUEST_MODE> GalleryBrowserTestInGuestMode;

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, OpenSingleImageOnDownloads) {
  set_test_case_name("openSingleImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       OpenSingleImageOnDownloads) {
  set_test_case_name("openSingleImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, OpenSingleImageOnDrive) {
  set_test_case_name("openSingleImageOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, OpenMultipleImagesOnDownloads) {
  set_test_case_name("openMultipleImagesOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       OpenMultipleImagesOnDownloads) {
  set_test_case_name("openMultipleImagesOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       OpenMultipleImagesAndSwitchToSlideModeOnDownloads) {
  set_test_case_name("openMultipleImagesAndChangeToSlideModeOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, OpenMultipleImagesOnDrive) {
  set_test_case_name("openMultipleImagesOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       CheckAvailabilityOfEditAndPrintButtons) {
  set_test_case_name("checkAvailabilityOfEditAndPrintButtons");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, TraverseSlideImagesOnDownloads) {
  set_test_case_name("traverseSlideImagesOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       TraverseSlideImagesOnDownloads) {
  set_test_case_name("traverseSlideImagesOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, TraverseSlideImagesOnDrive) {
  set_test_case_name("traverseSlideImagesOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       TraverseSlideThumbnailsOnDownloads) {
  set_test_case_name("traverseSlideThumbnailsOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, TraverseSlideThumbnailsOnDownloads) {
  set_test_case_name("traverseSlideThumbnailsOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, TraverseSlideThumbnailsOnDrive) {
  set_test_case_name("traverseSlideThumbnailsOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, RenameImageOnDownloads) {
  set_test_case_name("renameImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode, RenameImageOnDownloads) {
  set_test_case_name("renameImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, RenameImageOnDrive) {
  set_test_case_name("renameImageOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, DeleteImageOnDownloads) {
  set_test_case_name("deleteImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode, DeleteImageOnDownloads) {
  set_test_case_name("deleteImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, DeleteImageOnDrive) {
  set_test_case_name("deleteImageOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       CheckAvailabilityOfShareButtonOnDownloads) {
  set_test_case_name("checkAvailabilityOfShareButtonOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       CheckAvailabilityOfShareButtonOnDownloads) {
  set_test_case_name("checkAvailabilityOfShareButtonOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       CheckAvailabilityOfShareButtonOnDrive) {
  set_test_case_name("checkAvailabilityOfShareButtonOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, RotateImageOnDownloads) {
  set_test_case_name("rotateImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode, RotateImageOnDownloads) {
  set_test_case_name("rotateImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, RotateImageOnDrive) {
  set_test_case_name("rotateImageOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, CropImageOnDownloads) {
  set_test_case_name("cropImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode, CropImageOnDownloads) {
  set_test_case_name("cropImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, CropImageOnDrive) {
  set_test_case_name("cropImageOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, ExposureImageOnDownloads) {
  set_test_case_name("exposureImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       ExposureImageOnDownloads) {
  set_test_case_name("exposureImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, ExposureImageOnDrive) {
  set_test_case_name("exposureImageOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, ResizeImageOnDownloads) {
  set_test_case_name("resizeImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode, ResizeImageOnDownloads) {
  set_test_case_name("resizeImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, ResizeImageOnDrive) {
  set_test_case_name("resizeImageOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       EnableDisableOverwriteOriginalCheckboxOnDownloads) {
  set_test_case_name("enableDisableOverwriteOriginalCheckboxOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       EnableDisableOverwriteOriginalCheckboxOnDrive) {
  set_test_case_name("enableDisableOverwriteOriginalCheckboxOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       RenameImageInThumbnailModeOnDownloads) {
  set_test_case_name("renameImageInThumbnailModeOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, RenameImageInThumbnailModeOnDrive) {
  set_test_case_name("renameImageInThumbnailModeOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       DeleteAllImagesInThumbnailModeOnDownloads) {
  set_test_case_name("deleteAllImagesInThumbnailModeOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       DeleteAllImagesInThumbnailModeOnDrive) {
  set_test_case_name("deleteAllImagesInThumbnailModeOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       DeleteAllImagesInThumbnailModeWithEnterKey) {
  set_test_case_name("deleteAllImagesInThumbnailModeWithEnterKey");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       DeleteAllImagesInThumbnailModeWithDeleteKey) {
  set_test_case_name("deleteAllImagesInThumbnailModeWithDeleteKey");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       EmptySpaceClickUnselectsInThumbnailModeOnDownloads) {
  set_test_case_name("emptySpaceClickUnselectsInThumbnailModeOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       EmptySpaceClickUnselectsInThumbnailModeOnDrive) {
  set_test_case_name("emptySpaceClickUnselectsInThumbnailModeOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       SelectMultipleImagesWithShiftKeyOnDownloads) {
  set_test_case_name("selectMultipleImagesWithShiftKeyOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       SelectAllImagesAfterImageDeletionOnDownloads) {
  set_test_case_name("selectAllImagesAfterImageDeletionOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       SlideshowTraversalOnDownloads) {
  set_test_case_name("slideshowTraversalOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, SlideshowTraversalOnDownloads) {
  set_test_case_name("slideshowTraversalOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, SlideshowTraversalOnDrive) {
  set_test_case_name("slideshowTraversalOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       StopStartSlideshowOnDownloads) {
  set_test_case_name("stopStartSlideshowOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, StopStartSlideshowOnDownloads) {
  set_test_case_name("stopStartSlideshowOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, StopStartSlideshowOnDrive) {
  set_test_case_name("stopStartSlideshowOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, ActivateVideoFromThumbnailMode) {
  set_test_case_name("activateVideoFromThumbnailMode");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, DeleteSingleOpenPhotoOnDownloads) {
  set_test_case_name("deleteSingleOpenPhotoOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, OneImageSlideshowNoPauseButtonOnDownloads) {
  set_test_case_name("oneImageSlideshowNoPauseButtonOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, OneImageSlideshowNoPauseButtonOnDrive) {
  set_test_case_name("oneImageSlideshowNoPauseButtonOnDrive");
  StartTest();
}

}  // namespace file_manager
