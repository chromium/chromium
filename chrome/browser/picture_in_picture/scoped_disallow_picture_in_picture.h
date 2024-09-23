// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_SCOPED_DISALLOW_PICTURE_IN_PICTURE_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_SCOPED_DISALLOW_PICTURE_IN_PICTURE_H_

// As long as at least one of these objects is alive, existing
// picture-in-picture windows will be closed and new ones will be blocked from
// opening.
class ScopedDisallowPictureInPicture {
 public:
  ScopedDisallowPictureInPicture();
  ScopedDisallowPictureInPicture(const ScopedDisallowPictureInPicture&) =
      delete;
  ScopedDisallowPictureInPicture& operator=(
      const ScopedDisallowPictureInPicture&) = delete;
  ~ScopedDisallowPictureInPicture();
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_SCOPED_DISALLOW_PICTURE_IN_PICTURE_H_
