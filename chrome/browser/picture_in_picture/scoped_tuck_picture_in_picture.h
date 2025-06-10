// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_SCOPED_TUCK_PICTURE_IN_PICTURE_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_SCOPED_TUCK_PICTURE_IN_PICTURE_H_

// As long as at least one of these objects is alive, new and existing
// picture-in-picture windows will be tucked to the side of the screen.
class ScopedTuckPictureInPicture {
 public:
  ScopedTuckPictureInPicture();
  ScopedTuckPictureInPicture(const ScopedTuckPictureInPicture&) = delete;
  ScopedTuckPictureInPicture& operator=(const ScopedTuckPictureInPicture&) =
      delete;
  ~ScopedTuckPictureInPicture();
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_SCOPED_TUCK_PICTURE_IN_PICTURE_H_
