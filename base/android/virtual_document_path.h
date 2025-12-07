// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_VIRTUAL_DOCUMENT_PATH_H_
#define BASE_ANDROID_VIRTUAL_DOCUMENT_PATH_H_

#include <jni.h>

#include <optional>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"

namespace base::files_internal {

// Represents and operates on a virtual path for Android's Storage Access
// Framework (SAF).
//
// `base::FilePath` can store path-like strings, including `content://` URIs.
// However, applying string manipulations (like `Append`) to a `FilePath`
// that holds a `content://` URI often results in an invalid URI, as these
// URIs are not simple hierarchical paths.
//
// To address this, the `/SAF/...` virtual path format was introduced. This
// format is specifically designed to be safely manipulated by `FilePath`'s
// string operations. The path can represent both file and directory paths.
//
// This class, `VirtualDocumentPath`, is an object representation of a
// complete and immutable virtual document path. It is created by parsing a
// `/SAF/...` string. The class itself does not support path manipulation; its
// role is to interpret the virtual document path and execute operations against
// it, such as resolving it to a content URI (`ResolveToContentUri`) or
// performing file I/O (`WriteFile`).
//
// The virtual path format it parses is:
// /SAF/<authority>/tree/<documentID>/<relativePath>
//
// USAGE
//
// This class is primarily intended for internal use within the `//base/files`
// file API implementation.
//
// Code outside of `//base/files` should remain unaware of
// `VirtualDocumentPath`. Path construction should be done using
// `base::FilePath`. The resulting `FilePath` can then be passed to
// `//base/files` helper functions which, internally, may use
// `VirtualDocumentPath::Parse()` to interpret the path and perform an
// operation.
//
// EXAMPLE (for //base/files developers)
//
// To operate on a SAF path, first construct the full path using
// `base::FilePath`, then parse it into a `VirtualDocumentPath` object.
//
// Convert the `FilePath` storing a document tree URI to a `FilePath` storing
// a virtual document path:
//
//     base::FilePath dir(
//       "content://com.android.externalstorage.documents/tree/primary:A%2FB");
//     base::FilePath dir_vp = *dir.ResolveToVirtualDocumentPath();
//
// Construct the full path string using FilePath:
//
//     base::FilePath file_vp = dir_vp.Append("c.txt");
//
// Parse the virtual document path string into a VirtualDocumentPath object:
//
//     VirtualDocumentPath file_vpath = *VirtualDocumentPath::Parse(
//       file_vp.value());
//
// Use the object to perform an operation:
//
//     file_vpath->WriteFile(some_data);
//
// To perform I/O via other Android APIs, the virtual path can be resolved to
// a `content://` URI using `ResolveToContentUri()`:
//
//     base::FilePath file(*file_vpath.ResolveToContentUri());
class VirtualDocumentPath {
 public:
  VirtualDocumentPath(const VirtualDocumentPath& path);
  VirtualDocumentPath& operator=(const VirtualDocumentPath& path);
  ~VirtualDocumentPath();

  // Parses virtual path "/SAF/..." to `VirtualDocumentPath` or resolves a tree
  // URI (a content URI that represents a document tree) into
  // `VirtualDocumentPath`.
  // See
  // https://developer.android.com/reference/android/provider/DocumentsContract
  // for more about document tree URIs.
  static std::optional<VirtualDocumentPath> Parse(const std::string& path);
  // Resolves it to a content URI. If the file does not exist, it will return
  // nullopt. If it returns a value, it will not be an empty string.
  std::optional<std::string> ResolveToContentUri() const;
  // Returns string representation of the instance. See the class level comment
  // for details.
  std::string ToString() const;

  // Makes directory represented by the virtual path.
  // It returns whether the directory has been successfully created. If the file
  // already exists, it does nothing and returns false.
  bool Mkdir(mode_t mode) const;

  // Writes data to the file represented by the virtual path. If the file
  // already exists its content is truncated first. It returns true if the data
  // has been successfully written, and false otherwise.
  bool WriteFile(span<const uint8_t> data) const;

  // Creates an empty file if it does not exist and its parent directory exists.
  // If the file exists or created, it returns a pair of two values where the
  // first value is the content URI, and the second is a bool which is true if
  // the file has been created and false if the file already existed.
  std::optional<std::pair<std::string, bool>> CreateOrOpen() const;

 private:
  explicit VirtualDocumentPath(const base::android::JavaRef<jobject>& obj);

  base::android::ScopedJavaGlobalRef<jobject> obj_;
};

}  // namespace base::files_internal

#endif  // BASE_ANDROID_VIRTUAL_DOCUMENT_PATH_H_
