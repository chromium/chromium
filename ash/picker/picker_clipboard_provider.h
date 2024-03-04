// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_CLIPBOARD_PROVIDER_H_
#define ASH_PICKER_PICKER_CLIPBOARD_PROVIDER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_clock.h"

namespace ash {

class ClipboardHistoryItem;
class PickerListItemView;
class PickerSearchResult;

// A provider to fetch clipboard history.
class ASH_EXPORT PickerClipboardProvider {
 public:
  // Indicates the user has selected a result.
  using SelectSearchResultCallback =
      base::RepeatingCallback<void(const PickerSearchResult& result)>;

  using OnFetchResultCallback =
      base::RepeatingCallback<void(std::unique_ptr<PickerListItemView>)>;

  explicit PickerClipboardProvider(
      SelectSearchResultCallback select_result_callback,
      base::Clock* clock = base::DefaultClock::GetInstance());

  PickerClipboardProvider(const PickerClipboardProvider&) = delete;
  PickerClipboardProvider& operator=(const PickerClipboardProvider&) = delete;
  ~PickerClipboardProvider();

  void FetchResult(OnFetchResultCallback callback);

 private:
  void OnFetchHistory(OnFetchResultCallback callback,
                      std::vector<ClipboardHistoryItem> items);

  SelectSearchResultCallback select_result_callback_;
  raw_ptr<base::Clock> clock_;
  base::WeakPtrFactory<PickerClipboardProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_CLIPBOARD_PROVIDER_H_
