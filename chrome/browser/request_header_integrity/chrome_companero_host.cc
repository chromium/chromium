// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/request_header_integrity/chrome_companero_host.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/scoped_thread_priority.h"
#include "build/build_config.h"
#include "chrome/common/request_header_integrity/chrome_companero.mojom.h"
#include "chrome/common/request_header_integrity/internal/integrity_seed_internal.h"
#include "components/embedder_support/user_agent_utils.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/http_request_headers.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#endif

namespace request_header_integrity {

namespace {

base::FilePath GetLibraryPath() {
#if BUILDFLAG(IS_MAC)
  base::FilePath framework_dir = base::apple::FrameworkBundlePath();
  return framework_dir.Append(FILE_PATH_LITERAL("Libraries"))
      .Append(FILE_PATH_LITERAL("libchromecompaneros.dylib"));
#else
  base::FilePath module_dir;
  if (!base::PathService::Get(base::DIR_MODULE, &module_dir)) {
    return base::FilePath();
  }
#if BUILDFLAG(IS_WIN)
  return module_dir.Append(FILE_PATH_LITERAL("chromecompaneros.dll"));
#else
  return module_dir.Append(FILE_PATH_LITERAL("libchromecompaneros.so"));
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(IS_MAC)
}

}  // namespace

// Backend worker class that owns native shared library loading, token
// extraction, and Mojo receivers on background ThreadPool tasks.
class ChromeCompaneroHost::Backend : public mojom::ChromeCompanero {
 public:
  Backend() {
    base::FilePath library_path = GetLibraryPath();
    if (library_path.empty()) {
      return;
    }

    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    library_ = base::ScopedNativeLibrary(library_path);
    if (!library_.is_valid()) {
      return;
    }

    get_companero_value_fn_ = reinterpret_cast<GetCompaneroValueFunc>(
        library_.GetFunctionPointer("GetCompaneroValue"));
    get_header_name_fn_ = reinterpret_cast<GetHeaderNameFunc>(
        library_.GetFunctionPointer("GetHeaderName"));

    if (!get_companero_value_fn_ || !get_header_name_fn_) {
      get_companero_value_fn_ = nullptr;
      get_header_name_fn_ = nullptr;
      library_.reset();
    }
  }

  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;
  ~Backend() override = default;

  void BindReceiver(mojo::PendingReceiver<mojom::ChromeCompanero> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // mojom::ChromeCompanero:
  void GetHeaderNameAndValue(GetHeaderNameAndValueCallback callback) override {
    std::move(callback).Run(GetHeaderNameAndValueSync());
  }

  // Generates a fresh token pair. Crashes the process if the library returns
  // invalid data. Returns null in case of error.
  network::mojom::HttpRequestHeaderKeyValuePairPtr GetHeaderNameAndValueSync() {
    if (!is_valid()) {
      return nullptr;
    }

    std::optional<std::string> header_name = GetHeaderNameFromLib();
    if (!header_name) {
      return nullptr;
    }
    CHECK(net::HttpUtil::IsValidHeaderName(*header_name));

    std::optional<std::string> header_value =
        GetHeaderValueFromLib(kIntegritySeed, google_apis::GetAPIKey(),
                              embedder_support::GetUserAgent());
    if (!header_value) {
      return nullptr;
    }
    CHECK(net::HttpUtil::IsValidHeaderValue(*header_value));

    return network::mojom::HttpRequestHeaderKeyValuePair::New(
        *std::move(header_name), *std::move(header_value));
  }

 private:
  // Function pointer types for the native DSO export functions.
  // `buffer_size_chars` specifies the capacity of `out_buffer` in characters
  // (including space for the null terminator).
  using GetCompaneroValueFunc = size_t (*)(const char* seed,
                                           size_t seed_len,
                                           const char* api_key,
                                           size_t api_key_len,
                                           const char* user_agent,
                                           size_t user_agent_len,
                                           char* out_buffer,
                                           size_t buffer_size_chars);
  using GetHeaderNameFunc = size_t (*)(char* out_buffer,
                                       size_t buffer_size_chars);

  // Returns true if initialization succeeded.
  bool is_valid() const { return library_.is_valid(); }

  std::optional<std::string> GetHeaderNameFromLib() {
    if (!is_valid()) {
      return std::nullopt;
    }

    char name_buffer[64];
    size_t written = get_header_name_fn_(name_buffer, std::size(name_buffer));
    if (written == 0) {
      return std::nullopt;
    }
    CHECK_LE(written, std::size(name_buffer));
    return std::string(name_buffer, written);
  }

  std::optional<std::string> GetHeaderValueFromLib(
      std::string_view seed,
      std::string_view api_key,
      std::string_view user_agent) {
    if (!is_valid()) {
      return std::nullopt;
    }

    char value_buffer[64];
    size_t written = get_companero_value_fn_(
        seed.data(), seed.length(), api_key.data(), api_key.length(),
        user_agent.data(), user_agent.length(), value_buffer,
        std::size(value_buffer));
    if (written == 0) {
      return std::nullopt;
    }
    CHECK_LE(written, std::size(value_buffer));
    return std::string(value_buffer, written);
  }

  mojo::ReceiverSet<mojom::ChromeCompanero> receivers_;
  base::ScopedNativeLibrary library_;
  GetCompaneroValueFunc get_companero_value_fn_ = nullptr;
  GetHeaderNameFunc get_header_name_fn_ = nullptr;
};

ChromeCompaneroHost::ChromeCompaneroHost() {
  if (base::ThreadPoolInstance::Get()) {
    backend_.emplace(base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
  }
}

ChromeCompaneroHost::~ChromeCompaneroHost() = default;

void ChromeCompaneroHost::BindReceiver(
    mojo::PendingReceiver<mojom::ChromeCompanero> receiver) {
  if (backend_) {
    backend_.AsyncCall(&Backend::BindReceiver).WithArgs(std::move(receiver));
  }
}

void ChromeCompaneroHost::GetHeaderNameAndValue(
    GetHeaderNameAndValueCallback callback) {
  if (backend_) {
    backend_.AsyncCall(&Backend::GetHeaderNameAndValueSync)
        .Then(std::move(callback));
  } else {
    std::move(callback).Run(nullptr);
  }
}

}  // namespace request_header_integrity
