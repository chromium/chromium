// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_jstest_base.h"

class GalleryJsTest : public FileManagerJsTestBase {
 protected:
  GalleryJsTest()
      : FileManagerJsTestBase(
            base::FilePath(FILE_PATH_LITERAL("ui/file_manager/gallery/js"))) {}
};

IN_PROC_BROWSER_TEST_F(GalleryJsTest, ImageEncoderTest) {
  RunTestURL("image_editor/image_encoder_unittest_gen.html");
}

// Disabled on ASan builds due to a consistent failure. https://crbug.com/762831
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ExifEncoderTest DISABLED_ExifEncoderTest
#else
#define MAYBE_ExifEncoderTest ExifEncoderTest
#endif
IN_PROC_BROWSER_TEST_F(GalleryJsTest, MAYBE_ExifEncoderTest) {
  RunTestURL("image_editor/exif_encoder_unittest_gen.html");
}

IN_PROC_BROWSER_TEST_F(GalleryJsTest, ImageViewTest) {
  RunTestURL("image_editor/image_view_unittest_gen.html");
}

IN_PROC_BROWSER_TEST_F(GalleryJsTest, EntryListWatcherTest) {
  RunTestURL("entry_list_watcher_unittest_gen.html");
}

IN_PROC_BROWSER_TEST_F(GalleryJsTest, GalleryUtilTest) {
  RunTestURL("gallery_util_unittest_gen.html");
}

IN_PROC_BROWSER_TEST_F(GalleryJsTest, GalleryItemTest) {
  RunTestURL("gallery_item_unittest_gen.html");
}

IN_PROC_BROWSER_TEST_F(GalleryJsTest, GalleryDataModelTest) {
  RunTestURL("gallery_data_model_unittest_gen.html");
}

IN_PROC_BROWSER_TEST_F(GalleryJsTest, RibbonTest) {
  RunTestURL("ribbon_unittest_gen.html");
}

IN_PROC_BROWSER_TEST_F(GalleryJsTest, SlideModeTest) {
  RunTestURL("slide_mode_unittest_gen.html");
}

IN_PROC_BROWSER_TEST_F(GalleryJsTest, DimmableUIControllerTest) {
  RunTestURL("dimmable_ui_controller_unittest_gen.html");
}
