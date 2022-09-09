// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/test_download_shelf.h"

#include "base/time/time.h"

TestDownloadShelf::TestDownloadShelf(Profile* profile)
    : DownloadShelf(nullptr, profile) {}

TestDownloadShelf::~TestDownloadShelf() = default;

bool TestDownloadShelf::IsShowing() const {
  return is_showing_;
}

bool TestDownloadShelf::IsClosing() const {
  return false;
}

views::View* TestDownloadShelf::GetView() {
  return nullptr;
}

void TestDownloadShelf::DoShowDownload(
    DownloadUIModel::DownloadUIModelPtr download) {
  did_add_download_ = true;
}

void TestDownloadShelf::DoOpen() {
  is_showing_ = true;
}

void TestDownloadShelf::DoClose() {
  is_showing_ = false;
}

void TestDownloadShelf::DoHide() {
  is_showing_ = false;
}

void TestDownloadShelf::DoUnhide() {
  is_showing_ = true;
}

base::TimeDelta TestDownloadShelf::GetTransientDownloadShowDelay() const {
  return base::TimeDelta();
}
