// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_CONCEPT_H_
#define ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_CONCEPT_H_

#include <vector>

#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "ash/webui/help_app_ui/search/search_concept.pb.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"

namespace ash::help_app {

// Handles persistence for the help app search. Responsible for saving
// persistence to disk and load persistence from disk.
class SearchConcept {
 public:
  using ReadCallback =
      base::OnceCallback<void(std::vector<mojom::SearchConceptPtr>)>;

  explicit SearchConcept(const base::FilePath& filepath);
  ~SearchConcept();

  SearchConcept(const SearchConcept&) = delete;
  SearchConcept& operator=(const SearchConcept&) = delete;

  // Get the search concepts from the persistence.
  // Should be called right after this class is constructed.
  void GetSearchConcepts(ReadCallback on_read);

  // Save new persistence to disk.
  // Should be called when the new concepts come.
  void UpdateSearchConcepts(
      const std::vector<mojom::SearchConceptPtr>& concepts);

 private:
  void OnProtoRead(ReadCallback on_read,
                   std::unique_ptr<SearchConceptProto> proto);

  // The path to the proto file.
  const base::FilePath file_path_;

  // The path to the temporary proto file.
  base::FilePath temp_file_path_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<SearchConcept> weak_factory_{this};
};

}  // namespace ash::help_app

#endif  // ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_CONCEPT_H_
