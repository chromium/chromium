// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/test/fake_recent_source.h"

#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/extend.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "fake_recent_source.h"
#include "net/base/mime_util.h"

namespace ash {

// Helper method for matching file type against the desired `file_type`.
bool MatchesFileType(const RecentFile& file, RecentSource::FileType file_type) {
  if (file_type == RecentSource::FileType::kAll) {
    return true;
  }

  std::string mime_type;
  if (!net::GetMimeTypeFromFile(file.url().path(), &mime_type)) {
    return false;
  }

  switch (file_type) {
    case RecentSource::FileType::kAudio:
      return net::MatchesMimeType("audio/*", mime_type);
    case RecentSource::FileType::kImage:
      return net::MatchesMimeType("image/*", mime_type);
    case RecentSource::FileType::kVideo:
      return net::MatchesMimeType("video/*", mime_type);
    default:
      return false;
  }
}

FileProducer::FileProducer(const base::TimeDelta& lag,
                           std::vector<RecentFile> files)
    : lag_(lag), files_(std::move(files)) {}

FileProducer::~FileProducer() = default;

void FileProducer::GetFiles(RecentSource::GetRecentFilesCallback callback) {
  timers_.emplace_back(std::make_unique<base::OneShotTimer>());
  timers_.back()->Start(
      FROM_HERE, lag_,
      base::BindOnce(&FileProducer::OnFilesReady, base::Unretained(this),
                     std::move(callback)));
}

void FileProducer::OnFilesReady(RecentSource::GetRecentFilesCallback callback) {
  std::move(callback).Run(files_);
}

FakeRecentSource::CallContext::CallContext(GetRecentFilesCallback callback,
                                           const Params& params)
    : callback(std::move(callback)), params(params) {}
FakeRecentSource::CallContext::CallContext(CallContext&& context)
    : callback(std::move(context.callback)),
      params(context.params),
      accumulator(std::move(context.accumulator)),
      active_producer_count(context.active_producer_count) {}
FakeRecentSource::CallContext::~CallContext() = default;

FakeRecentSource::FakeRecentSource()
    : FakeRecentSource(
          extensions::api::file_manager_private::VolumeType::kTesting) {}

FakeRecentSource::FakeRecentSource(
    extensions::api::file_manager_private::VolumeType volume_type)
    : RecentSource(volume_type) {}

FakeRecentSource::~FakeRecentSource() = default;

void FakeRecentSource::AddProducer(std::unique_ptr<FileProducer> producer) {
  producers_.emplace_back(std::move(producer));
}

void FakeRecentSource::GetRecentFiles(const Params& params,
                                      GetRecentFilesCallback callback) {
  const auto& [it, _] = context_map_.emplace(
      params.call_id(), CallContext(std::move(callback), params));

  if (producers_.empty()) {
    OnAllProducersDone(params.call_id());
    return;
  }

  it->second.active_producer_count = producers_.size();
  for (const auto& producer : producers_) {
    producer->GetFiles(base::BindOnce(&FakeRecentSource::OnProducerReady,
                                      base::Unretained(this),
                                      params.call_id()));
  }
}

std::vector<RecentFile> FakeRecentSource::Stop(const int32_t call_id) {
  auto it = context_map_.find(call_id);
  if (it == context_map_.end()) {
    LOG(INFO) << "Received files after results delivered to recent model";
    return {};
  }

  std::vector<RecentFile> files =
      GetMatchingFiles(it->second.accumulator, it->second.params);
  context_map_.erase(it);
  return files;
}

void FakeRecentSource::OnProducerReady(const int32_t call_id,
                                       std::vector<RecentFile> files) {
  auto it = context_map_.find(call_id);
  if (it == context_map_.end()) {
    LOG(INFO) << "Producer ready after source stopped";
    return;
  }
  base::Extend(it->second.accumulator, files);
  if (--it->second.active_producer_count <= 0) {
    OnAllProducersDone(call_id);
  }
}

void FakeRecentSource::OnAllProducersDone(int32_t call_id) {
  auto it = context_map_.find(call_id);
  if (it == context_map_.end()) {
    LOG(WARNING) << "FakeRecentSource ready after it was stopped";
    return;
  }
  std::move(it->second.callback)
      .Run(GetMatchingFiles(it->second.accumulator, it->second.params));
  context_map_.erase(it);
}

std::vector<RecentFile> FakeRecentSource::GetMatchingFiles(
    const std::vector<RecentFile>& accumulator,
    const Params& params) {
  const std::u16string q16 = base::UTF8ToUTF16(params.query());
  std::vector<RecentFile> result;
  for (const auto& file : accumulator) {
    if (!MatchesFileType(file, params.file_type())) {
      continue;
    }
    if (!FileNameMatches(file.url().path().LossyDisplayName(), q16)) {
      continue;
    }
    result.push_back(file);
  }
  return result;
}

}  // namespace ash
