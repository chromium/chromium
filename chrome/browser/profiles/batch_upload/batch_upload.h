// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_H_
#define CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_H_

class Browser;
class Profile;
enum class BatchUploadDataType;

// Attempts to open the Batch Upload modal dialog that allows uploading the
// local profile data. The dialog will only be opened if there are some local
// data (of any type) to show. Retrurns whether the dialog was shown or not.
bool OpenBatchUpload(Browser* browser);

// Allows to know if a specific data type should have its BatchUpload entry
// point (access to the Batch Upload dialog) displayed. This performs the check
// on the specific requested type, and not the rest of the available types,
// meaning that if other types have local data to be displayed but not the
// requested one, the entry point should not be shown.
bool ShouldShowBatchUploadEntryPointForDataType(Profile& profile,
                                                BatchUploadDataType type);

#endif  // CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_H_
