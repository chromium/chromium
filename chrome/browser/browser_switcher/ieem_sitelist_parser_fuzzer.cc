// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "mojo/core/embedder/embedder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"

namespace browser_switcher {
namespace {

// Performs common initialization and holds state that's shared between all
// runs.
struct Environment {
  Environment() { mojo::core::Init(); }

  base::SingleThreadTaskExecutor main_thread_task_executor_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  FuzzedDataProvider data_provider(data, size);

  const auto parsing_mode = data_provider.ConsumeEnum<ParsingMode>();
  const auto xml = data_provider.ConsumeRemainingBytesAsString();

  base::RunLoop run_loop;
  ParseIeemXml(xml, parsing_mode,
               base::IgnoreArgs<ParsedXml>(run_loop.QuitClosure()));
  run_loop.Run();

  return 0;
}

}  // namespace
}  // namespace browser_switcher
