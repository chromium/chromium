// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/reading_list/reading_list_api.h"

#include "base/time/time.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/common/extensions/api/reading_list.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "extensions/browser/extension_function.h"
#include "url/gurl.h"

namespace extensions {

ReadingListAddEntryFunction::ReadingListAddEntryFunction() = default;
ReadingListAddEntryFunction::~ReadingListAddEntryFunction() = default;

ExtensionFunction::ResponseAction ReadingListAddEntryFunction::Run() {
  auto params = api::reading_list::AddEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  title_ = std::move(params->entry.title);
  url_ = GURL(params->entry.url);
  if (!url_.is_valid()) {
    return RespondNow(Error("URL is not valid"));
  }

  reading_list_model_ =
      ReadingListModelFactory::GetForBrowserContext(browser_context());

  if (!reading_list_model_->loaded()) {
    reading_list_observation_.Observe(reading_list_model_);
    AddRef();
    return RespondLater();
  }

  auto response = AddEntryToReadingList();
  return RespondNow(std::move(response));
}

void ReadingListAddEntryFunction::ReadingListModelLoaded(
    const ReadingListModel* model) {
  reading_list_observation_.Reset();
  auto response = AddEntryToReadingList();
  Respond(std::move(response));
  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseValue
ReadingListAddEntryFunction::AddEntryToReadingList() {
  if (!reading_list_model_->IsUrlSupported(url_)) {
    return Error("URL is not supported");
  }

  if (reading_list_model_->GetEntryByURL(url_)) {
    return Error("Duplicate URL");
  }

  reading_list_model_->AddOrReplaceEntry(
      url_, title_, reading_list::EntrySource::ADDED_VIA_EXTENSION,
      /*estimated_read_time=*/base::TimeDelta());

  return NoArguments();
}

}  // namespace extensions
