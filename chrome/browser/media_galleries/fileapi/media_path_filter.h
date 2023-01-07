// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_PATH_FILTER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_PATH_FILTER_H_

#include <stddef.h>

#include <map>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"

enum MediaGalleryFileType {
  MEDIA_GALLERY_FILE_TYPE_UNKNOWN = 0,
  MEDIA_GALLERY_FILE_TYPE_AUDIO = 1 << 0,
  MEDIA_GALLERY_FILE_TYPE_IMAGE = 1 << 1,
  MEDIA_GALLERY_FILE_TYPE_VIDEO = 1 << 2,
};

// This class holds the list of file path extensions that we should expose on
// media filesystem.
class MediaPathFilter {
 public:
  // Used to skip hidden folders and files. Returns true if the file specified
  // by |path| should be skipped.
  static bool ShouldSkip(const base::FilePath& path);

  MediaPathFilter();

  MediaPathFilter(const MediaPathFilter&) = delete;
  MediaPathFilter& operator=(const MediaPathFilter&) = delete;

  ~MediaPathFilter();

  // Returns true if |path| is a media file.
  bool Match(const base::FilePath& path);

  // Returns the type of |path| or MEDIA_GALLERY_FILE_TYPE_UNKNOWN if it
  // is not a media file.
  MediaGalleryFileType GetType(const base::FilePath& path);

 private:
  typedef std::vector<base::FilePath::StringType> MediaFileExtensionList;

  // Key: .extension
  // Value: MediaGalleryFileType, but stored as an int to allow "|="
  using MediaFileExtensionMap = std::map<base::FilePath::StringType, int>;

  void EnsureInitialized();

  void AddExtensionsToMediaFileExtensionMap(
      const MediaFileExtensionList& extensions_list,
      MediaGalleryFileType type);
  void AddAdditionalExtensionsToMediaFileExtensionMap(
      const base::FilePath::CharType* const* extensions_list,
      size_t extensions_list_size,
      MediaGalleryFileType type);
  void AddExtensionToMediaFileExtensionMap(
      const base::FilePath::CharType* extension,
      MediaGalleryFileType type);

  // Checks |initialized_| is only accessed on one sequence.
  SEQUENCE_CHECKER(sequence_checker_);
  bool initialized_;
  MediaFileExtensionMap media_file_extensions_map_;
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_PATH_FILTER_H_
