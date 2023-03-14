// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/search/search_concept.h"

#include <cstddef>
#include <iostream>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"

namespace ash::help_app {

namespace {

using Concept = SearchConceptProto::Concept;

constexpr char kReadHistogram[] =
    "Discover.SearchConcept.PersistenceReadStatus";
constexpr char kWriteHistogram[] =
    "Discover.SearchConcept.PersistenceWriteStatus";

// The result of reading a backing file from disk. These values persist to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class ReadStatus {
  kOk = 0,
  kMissing = 1,
  kReadError = 2,
  kParseError = 3,
  kMaxValue = kParseError,
};

// The result of writing a backing file to disk. These values persist to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class WriteStatus {
  kOk = 0,
  kWriteError = 1,
  kSerializationError = 2,
  kReplaceError = 3,
  kMaxValue = kReplaceError,
};

// This should be incremented whenever a change to the search concept is made
// that is incompatible with on-disk state. On reading, any state is wiped if
// its version doesn't match.
constexpr int32_t kVersion = 1;

// Read proto from the disk.
std::unique_ptr<SearchConceptProto> ProtoRead(const base::FilePath& file_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!base::PathExists(file_path)) {
    base::UmaHistogramEnumeration(kReadHistogram, ReadStatus::kMissing);
    return nullptr;
  }

  std::string proto_str;
  if (!base::ReadFileToString(file_path, &proto_str)) {
    base::UmaHistogramEnumeration(kReadHistogram, ReadStatus::kReadError);
    return nullptr;
  }

  auto proto = std::make_unique<SearchConceptProto>();
  if (!proto->ParseFromString(proto_str)) {
    base::UmaHistogramEnumeration(kReadHistogram, ReadStatus::kParseError);
    return nullptr;
  }
  base::UmaHistogramEnumeration(kReadHistogram, ReadStatus::kOk);

  // Discard the proto if the version does not match.
  if (!proto->has_version() || proto->version() != kVersion) {
    base::DeleteFile(file_path);
    return nullptr;
  }

  return proto;
}

// Write proto to the disk.
void ProtoWrite(std::unique_ptr<SearchConceptProto> proto,
                const base::FilePath& file_path,
                const base::FilePath& temp_file_path) {
  std::string proto_str;
  if (!proto->SerializeToString(&proto_str)) {
    base::UmaHistogramEnumeration(kWriteHistogram,
                                  WriteStatus::kSerializationError);
    return;
  }

  const auto directory = temp_file_path.DirName();
  if (!base::DirectoryExists(directory)) {
    base::CreateDirectory(directory);
  }

  // Write temporary proto to `temp_file_path_`.
  bool write_succeed;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    write_succeed = base::ImportantFileWriter::WriteFileAtomically(
        temp_file_path, proto_str, "HelpAppPersistentProto");
  }

  if (!write_succeed) {
    base::UmaHistogramEnumeration(kWriteHistogram, WriteStatus::kWriteError);
    return;
  }

  // Replace the proto in `file_path_` by the temporary proto if the write is
  // succeed.
  const bool replace_succeed =
      base::ReplaceFile(temp_file_path, file_path, nullptr);
  base::UmaHistogramEnumeration(
      kWriteHistogram,
      replace_succeed ? WriteStatus::kOk : WriteStatus::kReplaceError);
}

}  // namespace

SearchConcept::SearchConcept(const base::FilePath& file_path)
    : file_path_(file_path),
      temp_file_path_(file_path.DirName().AppendASCII("tmp.pb")),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

SearchConcept::~SearchConcept() = default;

void SearchConcept::GetSearchConcepts(ReadCallback on_read) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ProtoRead, file_path_),
      base::BindOnce(&SearchConcept::OnProtoRead, weak_factory_.GetWeakPtr(),
                     base::BindPostTaskToCurrentDefault(std::move(on_read))));
}

void SearchConcept::UpdateSearchConcepts(
    const std::vector<mojom::SearchConceptPtr>& search_concepts) {
  // Ignore the request if the SearchConcepts is empty.
  if (search_concepts.empty()) {
    return;
  }

  std::unique_ptr<SearchConceptProto> proto =
      std::make_unique<SearchConceptProto>();
  proto->set_version(kVersion);
  auto& proto_concepts = *proto->mutable_concepts();

  for (const auto& search_concept : search_concepts) {
    Concept& proto_concept = *proto_concepts.Add();
    proto_concept.set_id(search_concept->id);
    proto_concept.set_title(base::UTF16ToUTF8(search_concept->title));
    proto_concept.set_main_category(
        base::UTF16ToUTF8(search_concept->main_category));

    auto& proto_tags = *proto_concept.mutable_tags();
    for (const auto& search_tag : search_concept->tags) {
      proto_tags.Add(base::UTF16ToUTF8(search_tag));
    }

    proto_concept.set_tag_locale(search_concept->tag_locale);
    proto_concept.set_url_path_with_parameters(
        search_concept->url_path_with_parameters);
    proto_concept.set_locale(search_concept->locale);
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ProtoWrite, std::move(proto), file_path_,
                                temp_file_path_));
}

void SearchConcept::OnProtoRead(ReadCallback on_read,
                                std::unique_ptr<SearchConceptProto> proto) {
  std::vector<mojom::SearchConceptPtr> search_concepts;
  if (!proto) {
    std::move(on_read).Run(std::move(search_concepts));
    return;
  }

  const auto proto_concepts = proto->concepts();

  if (proto_concepts.empty()) {
    std::move(on_read).Run(std::move(search_concepts));
    return;
  }

  // convert the concepts of proto into
  // `std::vector<mojom::SearchConceptPtr>`.
  for (const auto& proto_concept : proto_concepts) {
    mojom::SearchConceptPtr search_concept = mojom::SearchConcept::New();
    search_concept->id = proto_concept.id();
    search_concept->title = base::UTF8ToUTF16(proto_concept.title());
    search_concept->main_category =
        base::UTF8ToUTF16(proto_concept.main_category());

    std::vector<::std::u16string> search_tags;
    for (const auto& tag : proto_concept.tags()) {
      search_tags.push_back(base::UTF8ToUTF16(tag));
    }
    search_concept->tags = search_tags;

    search_concept->tag_locale = proto_concept.tag_locale();
    search_concept->url_path_with_parameters =
        proto_concept.url_path_with_parameters();
    search_concept->locale = proto_concept.locale();

    search_concepts.push_back(std::move(search_concept));
  }

  std::move(on_read).Run(std::move(search_concepts));
}

}  // namespace ash::help_app
