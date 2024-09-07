// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_AUTH_HEADER_VIEW_H_
#define ASH_AUTH_VIEWS_AUTH_HEADER_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/login/ui/animated_rounded_image_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/account_id/account_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

// Contains the user avatar, title and description.
// The title can display error message.
// Provide function to restore the original title.
class ASH_EXPORT AuthHeaderView : public views::View {
  METADATA_HEADER(AuthHeaderView, views::View)

 public:
  class TestApi {
   public:
    explicit TestApi(AuthHeaderView* view);
    ~TestApi();
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    const std::u16string& GetCurrentTitle() const;

    raw_ptr<AuthHeaderView> GetView();

   private:
    const raw_ptr<AuthHeaderView> view_;
  };

  class Observer : public base::CheckedObserver {
   public:
    Observer();
    ~Observer() override;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual void OnTitleChanged(const std::u16string& error_str) = 0;
  };

  AuthHeaderView(const AccountId& account_id,
                 const std::u16string& title,
                 const std::u16string& description);

  AuthHeaderView(const AuthHeaderView&) = delete;
  AuthHeaderView& operator=(const AuthHeaderView&) = delete;

  ~AuthHeaderView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  base::WeakPtr<AuthHeaderView> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SetErrorTitle(const std::u16string& error_str);
  void RestoreTitle();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void NotifyTitleChanged(const std::u16string& title);

  raw_ptr<AnimatedRoundedImageView> avatar_view_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> description_label_ = nullptr;

  const std::u16string title_str_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<AuthHeaderView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_AUTH_HEADER_VIEW_H_
