// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_DATA_REMOVAL_DIALOG_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_DATA_REMOVAL_DIALOG_H_

#include "base/functional/callback_forward.h"

class Profile;

namespace arc {

// Callback to notify the result of data removal confirmation dialog. Invoked
// with true in case the user accepts data removal and false in case the user
// declines data removal or dialog is closed.
using DataRemovalConfirmationCallback = base::OnceCallback<void(bool)>;

// It shows a confirmation dialog for data removal. User has the option to
// accept or decline data removal. The result is passed using |callback|.
void ShowDataRemovalConfirmationDialog(
    Profile* profile,
    DataRemovalConfirmationCallback callback);

// Returns true if the ARC data removal dialog is open.
bool IsDataRemovalConfirmationDialogOpenForTesting();

// Simulates that user closes the open confirmation dialog for data removal.
// The confirmation dialog is closed and |confirm| is returned in confirmation
// callback, which was passed during ShowDataRemovalConfirmationDialog call.
void CloseDataRemovalConfirmationDialogForTesting(bool confirm);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_DATA_REMOVAL_DIALOG_H_
