// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_CLIPBOARD_HISTORY_PROVIDER_H_
#define ASH_PICKER_PICKER_CLIPBOARD_HISTORY_PROVIDER_H_

#include <memory>
#include <string>
#include <string_view>

#include "ash/ash_export.h"
#include "ash/picker/picker_search_result.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_clock.h"

namespace ash {

class ClipboardHistoryItem;

// A provider to fetch clipboard history.
class ASH_EXPORT PickerClipboardHistoryProvider {
 public:
  using OnFetchResultsCallback =
      base::OnceCallback<void(std::vector<PickerSearchResult>)>;

  explicit PickerClipboardHistoryProvider(
      base::Clock* clock = base::DefaultClock::GetInstance());

  PickerClipboardHistoryProvider(const PickerClipboardHistoryProvider&) =
      delete;
  PickerClipboardHistoryProvider& operator=(
      const PickerClipboardHistoryProvider&) = delete;
  ~PickerClipboardHistoryProvider();

  // Fetches clipboard items which were copied within `recency` time duration.
  void FetchResults(OnFetchResultsCallback callback,
                    std::u16string_view query = u"");

 private:
  void OnFetchHistory(OnFetchResultsCallback callback,
                      std::u16string query,
                      std::vector<ClipboardHistoryItem> items);

  raw_ptr<base::Clock> clock_;
  base::WeakPtrFactory<PickerClipboardHistoryProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_CLIPBOARD_HISTORY_PROVIDER_H_
