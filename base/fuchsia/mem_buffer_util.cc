// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/mem_buffer_util.h"

#include <lib/fdio/io.h>

#include <lib/zx/vmo.h>
#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"

namespace base {

absl::optional<std::u16string> ReadUTF8FromVMOAsUTF16(
    const fuchsia::mem::Buffer& buffer) {
  absl::optional<std::string> output_utf8 = StringFromMemBuffer(buffer);
  if (!output_utf8)
    return absl::nullopt;
  std::u16string output;
  return UTF8ToUTF16(&output_utf8->front(), output_utf8->size(), &output)
             ? absl::optional<std::u16string>(std::move(output))
             : absl::nullopt;
}

zx::vmo VmoFromString(StringPiece data, StringPiece name) {
  zx::vmo vmo;

  // The `ZX_PROP_VMO_CONTENT_SIZE` property is automatically set on VMO
  // creation.
  zx_status_t status = zx::vmo::create(data.size(), 0, &vmo);
  ZX_CHECK(status == ZX_OK, status) << "zx_vmo_create";
  status = vmo.set_property(ZX_PROP_NAME, name.data(), name.size());
  ZX_DCHECK(status == ZX_OK, status);
  if (data.size() > 0) {
    status = vmo.write(data.data(), 0, data.size());
    ZX_CHECK(status == ZX_OK, status) << "zx_vmo_write";
  }
  return vmo;
}

fuchsia::mem::Buffer MemBufferFromString(StringPiece data, StringPiece name) {
  fuchsia::mem::Buffer buffer;
  buffer.vmo = VmoFromString(data, name);
  buffer.size = data.size();
  return buffer;
}

fuchsia::mem::Buffer MemBufferFromString16(StringPiece16 data,
                                           StringPiece name) {
  return MemBufferFromString(
      StringPiece(reinterpret_cast<const char*>(data.data()),
                  data.size() * sizeof(char16_t)),
      name);
}

absl::optional<std::string> StringFromVmo(const zx::vmo& vmo) {
  std::string result;

  size_t size;
  zx_status_t status = vmo.get_prop_content_size(&size);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx::vmo::get_prop_content_size";
    return absl::nullopt;
  }

  if (size == 0)
    return result;

  result.resize(size);
  status = vmo.read(&result[0], 0, size);
  if (status == ZX_OK)
    return result;

  ZX_LOG(ERROR, status) << "zx_vmo_read";
  return absl::nullopt;
}

absl::optional<std::string> StringFromMemBuffer(
    const fuchsia::mem::Buffer& buffer) {
  std::string result;

  if (buffer.size == 0)
    return result;

  result.resize(buffer.size);
  zx_status_t status = buffer.vmo.read(&result[0], 0, buffer.size);
  if (status == ZX_OK)
    return result;

  ZX_LOG(ERROR, status) << "zx_vmo_read";
  return absl::nullopt;
}

absl::optional<std::string> StringFromMemData(const fuchsia::mem::Data& data) {
  switch (data.Which()) {
    case fuchsia::mem::Data::kBytes: {
      const std::vector<uint8_t>& bytes = data.bytes();
      return std::string(bytes.begin(), bytes.end());
    }
    case fuchsia::mem::Data::kBuffer:
      return StringFromMemBuffer(data.buffer());
    case fuchsia::mem::Data::kUnknown:
    case fuchsia::mem::Data::Invalid:
      // TODO(fxbug.dev/66155): Determine whether to use a default case instead.
      break;
  }

  return absl::nullopt;
}

fuchsia::mem::Buffer MemBufferFromFile(File file) {
  if (!file.IsValid())
    return {};

  zx::vmo vmo;
  zx_status_t status =
      fdio_get_vmo_copy(file.GetPlatformFile(), vmo.reset_and_get_address());
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "fdio_get_vmo_copy";
    return {};
  }

  fuchsia::mem::Buffer output;
  output.vmo = std::move(vmo);
  output.size = checked_cast<uint64_t>(file.GetLength());
  return output;
}

fuchsia::mem::Buffer CloneBuffer(const fuchsia::mem::Buffer& buffer,
                                 StringPiece name) {
  fuchsia::mem::Buffer output;
  output.size = buffer.size;
  zx_status_t status = buffer.vmo.create_child(
      ZX_VMO_CHILD_SNAPSHOT_AT_LEAST_ON_WRITE, 0, buffer.size, &output.vmo);
  ZX_CHECK(status == ZX_OK, status) << "zx_vmo_create_child";

  status = output.vmo.set_property(ZX_PROP_NAME, name.data(), name.size());
  ZX_DCHECK(status == ZX_OK, status);

  return output;
}

}  // namespace base
