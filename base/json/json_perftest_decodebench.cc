// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This program measures the time taken to decode the given JSON files (the
// command line arguments). It is for manual benchmarking.
//
// Usage:
// $ ninja -C out/foobar json_perftest_decodebench
// $ out/foobar/json_perftest_decodebench -a -n=10 the/path/to/your/*.json
//
// The -n=10 switch controls the number of iterations. It defaults to 1.
//
// The -a switch means to print 1 non-comment line per input file (the average
// iteration time). Without this switch (the default), it prints n non-comment
// lines per input file (individual iteration times). For a single input file,
// building and running this program before and after a particular commit can
// work well with the 'ministat' tool: https://github.com/thorduri/ministat

#include <inttypes.h>
#include <iomanip>
#include <iostream>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/time/time.h"

int main(int argc, char* argv[]) {
  if (!base::ThreadTicks::IsSupported()) {
    std::cout << "# base::ThreadTicks is not supported\n";
    return EXIT_FAILURE;
  }
  base::ThreadTicks::WaitUntilInitialized();

  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool average = command_line->HasSwitch("a");
  int iterations = 1;
  std::string iterations_str = command_line->GetSwitchValueASCII("n");
  if (!iterations_str.empty()) {
    iterations = atoi(iterations_str.c_str());
    if (iterations < 1) {
      std::cout << "# invalid -n command line switch\n";
      return EXIT_FAILURE;
    }
  }

  if (average) {
    std::cout << "# Microseconds (μs), n=" << iterations << ", averaged"
              << std::endl;
  } else {
    std::cout << "# Microseconds (μs), n=" << iterations << std::endl;
  }
  for (const auto& filename : command_line->GetArgs()) {
    std::string src;
    if (!base::ReadFileToString(base::FilePath(filename), &src)) {
      std::cout << "# could not read " << filename << std::endl;
      return EXIT_FAILURE;
    }

    int64_t total_time = 0;
    std::string error_message;
    for (int i = 0; i < iterations; ++i) {
      auto start = base::ThreadTicks::Now();
      auto v = base::JSONReader::ReadAndReturnValueWithError(src);
      auto end = base::ThreadTicks::Now();
      int64_t iteration_time = (end - start).InMicroseconds();
      total_time += iteration_time;

      if (i == 0) {
        if (average) {
          error_message = std::move(v.error_message);
        } else {
          std::cout << "# " << filename;
          if (!v.error_message.empty()) {
            std::cout << ": " << v.error_message;
          }
          std::cout << std::endl;
        }
      }

      if (!average) {
        std::cout << iteration_time << std::endl;
      }
    }

    if (average) {
      int64_t average_time = total_time / iterations;
      std::cout << std::setw(12) << average_time << "\t# " << filename;
      if (!error_message.empty()) {
        std::cout << ": " << error_message;
      }
      std::cout << std::endl;
    }
  }
  return EXIT_SUCCESS;
}
