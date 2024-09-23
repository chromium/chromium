// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/test/uploader_test_utils.h"

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors::test {

std::string GetBodyFromFileOrPageRequest(
    safe_browsing::ConnectorDataPipeGetter* data_pipe_getter) {
  EXPECT_TRUE(data_pipe_getter);

  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;

  base::RunLoop run_loop;
  EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, data_pipe_producer,
                                                 data_pipe_consumer));
  // Read data from `data_pipe_getter` and write it to `data_pipe_producer`.
  data_pipe_getter->Read(
      std::move(data_pipe_producer),
      base::BindLambdaForTesting([&run_loop](int32_t status, uint64_t size) {
        EXPECT_EQ(net::OK, status);
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_TRUE(data_pipe_consumer.is_valid());
  std::string body;
  // Write data from `data_pipe_consumer` to `buffer`, and ultimately to `body`.
  while (true) {
    std::string buffer(1024, '\0');
    size_t read_size = 0;
    MojoResult result = data_pipe_consumer->ReadData(
        MOJO_READ_DATA_FLAG_NONE, base::as_writable_byte_span(buffer),
        read_size);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop().RunUntilIdle();
      continue;
    }
    if (result != MOJO_RESULT_OK) {
      break;
    }
    body.append(std::string_view(buffer).substr(0, read_size));
  }

  return body;
}

}  // namespace enterprise_connectors::test
