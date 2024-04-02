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
class PickerSearchResult;

// A provider to fetch clipboard history.
class ASH_EXPORT PickerClipboardProvider {
 public:
  using OnFetchResultsCallback =
      base::OnceCallback<void(std::vector<PickerSearchResult>)>;

  explicit PickerClipboardProvider(
      base::Clock* clock = base::DefaultClock::GetInstance());

  PickerClipboardProvider(const PickerClipboardProvider&) = delete;
  PickerClipboardProvider& operator=(const PickerClipboardProvider&) = delete;
  ~PickerClipboardProvider();

  // Fetches clipboard items which were copied within `recency` time duration.
  void FetchResults(OnFetchResultsCallback callback,
                    const std::u16string& query = u"",
                    base::TimeDelta recency = base::TimeDelta::Max());

 private:
  void OnFetchHistory(OnFetchResultsCallback callback,
                      const std::u16string& query,
                      base::TimeDelta recency,
                      std::vector<ClipboardHistoryItem> items);

  raw_ptr<base::Clock> clock_;
  base::WeakPtrFactory<PickerClipboardProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_CLIPBOARD_PROVIDER_H_
