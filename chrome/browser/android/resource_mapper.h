// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_RESOURCE_MAPPER_H_
#define CHROME_BROWSER_ANDROID_RESOURCE_MAPPER_H_

// Enumerates IDs of resources used in the Android port of Chromium.  This is
// needed so that Android knows which Drawable is needed in the Java UI.
class ResourceMapper {
 public:
  // ID indicating that the map failed to find a Drawable corresponding to the
  // Chromium resource.
  static const int kMissingId;

  // Converts the given chromium |resource_id| (e.g. IDR_INFOBAR_TRANSLATE) to
  // an Android drawable resource ID. Returns |kMissingId| if a mapping wasn't
  // found.
  static int MapToJavaDrawableId(int resource_id);

 private:
  // Create the mapping.  IDs start at 0 to correspond to the array that gets
  // built in the corresponding ResourceID Java class.
  static void ConstructMap();
};

#endif  // CHROME_BROWSER_ANDROID_RESOURCE_MAPPER_H_
