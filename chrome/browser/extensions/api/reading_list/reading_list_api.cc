// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/reading_list/reading_list_api.h"

#include "base/time/time.h"
#include "chrome/browser/extensions/api/reading_list/reading_list_api_constants.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/common/extensions/api/reading_list.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "extensions/browser/extension_function.h"
#include "url/gurl.h"

namespace extensions {

//////////////////////////////////////////////////////////////////////////////
////////////////////////ReadingListAddEntryFunction///////////////////////////
//////////////////////////////////////////////////////////////////////////////

ReadingListAddEntryFunction::ReadingListAddEntryFunction() = default;
ReadingListAddEntryFunction::~ReadingListAddEntryFunction() = default;

ExtensionFunction::ResponseAction ReadingListAddEntryFunction::Run() {
  auto params = api::reading_list::AddEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  title_ = std::move(params->entry.title);
  url_ = GURL(params->entry.url);
  if (!url_.is_valid()) {
    return RespondNow(Error(reading_list_api_constants::kInvalidURLError));
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
    return Error(reading_list_api_constants::kNotSupportedURLError);
  }

  if (reading_list_model_->GetEntryByURL(url_)) {
    return Error(reading_list_api_constants::kDuplicateURLError);
  }

  reading_list_model_->AddOrReplaceEntry(
      url_, title_, reading_list::EntrySource::ADDED_VIA_EXTENSION,
      /*estimated_read_time=*/base::TimeDelta());

  return NoArguments();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////ReadingListRemoveEntryFunction//////////////////////////
//////////////////////////////////////////////////////////////////////////////

ReadingListRemoveEntryFunction::ReadingListRemoveEntryFunction() = default;
ReadingListRemoveEntryFunction::~ReadingListRemoveEntryFunction() = default;

ExtensionFunction::ResponseAction ReadingListRemoveEntryFunction::Run() {
  auto params = api::reading_list::RemoveEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  url_ = GURL(params->info.url);
  if (!url_.is_valid()) {
    return RespondNow(Error(reading_list_api_constants::kInvalidURLError));
  }

  reading_list_model_ =
      ReadingListModelFactory::GetForBrowserContext(browser_context());

  if (!reading_list_model_->loaded()) {
    reading_list_observation_.Observe(reading_list_model_);
    AddRef();
    return RespondLater();
  }

  auto response = RemoveEntryFromReadingList();
  return RespondNow(std::move(response));
}

void ReadingListRemoveEntryFunction::ReadingListModelLoaded(
    const ReadingListModel* model) {
  reading_list_observation_.Reset();
  auto response = RemoveEntryFromReadingList();
  Respond(std::move(response));
  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseValue
ReadingListRemoveEntryFunction::RemoveEntryFromReadingList() {
  if (!reading_list_model_->IsUrlSupported(url_)) {
    return Error(reading_list_api_constants::kNotSupportedURLError);
  }

  if (!reading_list_model_->GetEntryByURL(url_)) {
    return Error(reading_list_api_constants::kURLNotFoundError);
  }

  reading_list_model_->RemoveEntryByURL(url_);

  return NoArguments();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////ReadingListUpdateEntryFunction//////////////////////////
//////////////////////////////////////////////////////////////////////////////

ReadingListUpdateEntryFunction::ReadingListUpdateEntryFunction() = default;
ReadingListUpdateEntryFunction::~ReadingListUpdateEntryFunction() = default;

ExtensionFunction::ResponseAction ReadingListUpdateEntryFunction::Run() {
  auto params = api::reading_list::UpdateEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  title_ = params->info.title;
  has_been_read_ = params->info.has_been_read;

  if (!title_.has_value() && !has_been_read_.has_value()) {
    return RespondNow(Error(reading_list_api_constants::kNoUpdateProvided));
  }

  url_ = GURL(params->info.url);
  if (!url_.is_valid()) {
    return RespondNow(Error(reading_list_api_constants::kInvalidURLError));
  }

  reading_list_model_ =
      ReadingListModelFactory::GetForBrowserContext(browser_context());

  if (!reading_list_model_->loaded()) {
    reading_list_observation_.Observe(reading_list_model_);
    AddRef();
    return RespondLater();
  }

  auto response = UpdateEntriesInTheReadingList();
  return RespondNow(std::move(response));
}

void ReadingListUpdateEntryFunction::ReadingListModelLoaded(
    const ReadingListModel* model) {
  reading_list_observation_.Reset();
  auto response = UpdateEntriesInTheReadingList();
  Respond(std::move(response));
  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseValue
ReadingListUpdateEntryFunction::UpdateEntriesInTheReadingList() {
  if (!reading_list_model_->IsUrlSupported(url_)) {
    return Error(reading_list_api_constants::kNotSupportedURLError);
  }

  if (!reading_list_model_->GetEntryByURL(url_)) {
    return Error(reading_list_api_constants::kURLNotFoundError);
  }

  if (title_.has_value()) {
    reading_list_model_->SetEntryTitleIfExists(url_, title_.value());
  }

  if (has_been_read_.has_value()) {
    reading_list_model_->SetReadStatusIfExists(url_, has_been_read_.value());
  }

  return NoArguments();
}

}  // namespace extensions
