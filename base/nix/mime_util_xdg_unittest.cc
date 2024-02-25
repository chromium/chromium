// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/nix/mime_util_xdg.h"

#include <map>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::nix {
namespace {

// Test mime.cache files are generated using a process such as:
// mkdir -p /tmp/mimetest/packages
// cat <<EOF >> /tmp/mimetest/packages/application-x-foobar.xml
// <?xml version="1.0" encoding="UTF-8"?>
// <mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
//   <mime-type type="x/no-dot"><glob pattern="~"/></mime-type>
//   <mime-type type="application/pdf"><glob pattern="*.pdf"/></mime-type>
//   <mime-type type="text/plain"><glob pattern="*.txt"/></mime-type>
//   <mime-type type="text/plain"><glob pattern="*.doc"/></mime-type>
//   <mime-type type="x/ignore"><glob pattern="*.foo" weight="60"/></mime-type>
//   <mime-type type="x/foo"><glob pattern="*.foo" weight="80"/></mime-type>
//   <mime-type type="text/plain"><glob pattern="*.foo"/></mime-type>
//   <mime-type type="x/smile"><glob pattern="*.🙂🤩"/></mime-type>
// </mime-info>
// EOF
// update-mime-database /tmp/mimetest
// base64 -w72 /tmp/mimetest/mime.cache
// See https://wiki.archlinux.org/title/XDG_MIME_Applications

constexpr char kTestMimeCacheB64[] =
    "AAEAAgAAAHQAAAB4AAAAfAAAAIwAAAHMAAAB0AAAAdwAAAHgAAAB5AAAAehhcHBsaWNhdGlv"
    "bi9wZGYAeC9zbWlsZQB4L2lnbm9yZQAAAAB0ZXh0L3BsYWluAAB4L2ZvbwAAAH4AAAB4L25v"
    "LWRvdAAAAAAAAAAAAAAAAAAAAAEAAABkAAAAaAAAADIAAAAFAAAAlAAAAGMAAAABAAAA0AAA"
    "AGYAAAABAAAA3AAAAG8AAAABAAAA6AAAAHQAAAABAAAA9AAB+SkAAAABAAABAAAAAG8AAAAB"
    "AAABDAAAAGQAAAABAAABGAAAAG8AAAABAAABJAAAAHgAAAABAAABMAAB9kIAAAABAAABPAAA"
    "AGQAAAABAAABSAAAAHAAAAABAAABVAAAAGYAAAABAAABYAAAAHQAAAABAAABbAAAAC4AAAAB"
    "AAABeAAAAC4AAAABAAABhAAAAC4AAAABAAABkAAAAC4AAAADAAABnAAAAC4AAAABAAABwAAA"
    "AAAAAAA8AAAAMgAAAAAAAABQAAAAMgAAAAAAAAAsAAAAMgAAAAAAAABEAAAAPAAAAAAAAABc"
    "AAAAUAAAAAAAAABQAAAAMgAAAAAAAABQAAAAMgAAAAAAAAAAAAAAAAAAAdwAAAAAAAAAAAAA"
    "AAAAAAAGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

class ParseMimeTypesTest : public ::testing::Test {
 public:
  ParseMimeTypesTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    mime_types_path_ = temp_dir_.GetPath().Append("mime.types");
  }
  ParseMimeTypesTest(const ParseMimeTypesTest&) = delete;
  ParseMimeTypesTest& operator=(const ParseMimeTypesTest&) = delete;

  ~ParseMimeTypesTest() override = default;

  // Ensures that parsing fails when mime.cache file is modified such that
  // `buf[pos] = c`.
  void InvalidIf(std::vector<uint8_t>& buf, size_t pos, uint8_t c) {
    ASSERT_LT(pos, buf.size());
    uint8_t old_c = buf[pos];
    buf[pos] = c;
    ASSERT_TRUE(base::WriteFile(TempFile(), buf));
    MimeTypeMap map;
    EXPECT_FALSE(ParseMimeTypes(TempFile(), map));
    buf[pos] = old_c;
  }

  const FilePath& TempFile() const { return mime_types_path_; }

 private:
  ScopedTempDir temp_dir_;
  FilePath mime_types_path_;
};

}  // namespace

bool operator==(const WeightedMime& lhs, const WeightedMime& rhs) {
  return lhs.mime_type == rhs.mime_type && lhs.weight == rhs.weight;
}

TEST_F(ParseMimeTypesTest, NonExistentFileFails) {
  MimeTypeMap map;
  EXPECT_FALSE(ParseMimeTypes(FilePath("/invalid/filepath/foo"), map));
}

TEST_F(ParseMimeTypesTest, ValidResult) {
  MimeTypeMap map;
  auto buf = Base64Decode(kTestMimeCacheB64);
  ASSERT_TRUE(buf.has_value());
  ASSERT_TRUE(WriteFile(TempFile(), *buf));
  EXPECT_TRUE(ParseMimeTypes(TempFile(), map));
  const MimeTypeMap kExpected = {
      {"pdf", {"application/pdf", 50}}, {"txt", {"text/plain", 50}},
      {"doc", {"text/plain", 50}},      {"foo", {"x/foo", 80}},
      {"🙂🤩", {"x/smile", 50}},
  };
  EXPECT_EQ(map, kExpected);
}

TEST_F(ParseMimeTypesTest, Empty) {
  MimeTypeMap map;
  ASSERT_TRUE(WriteFile(TempFile(), ""));
  EXPECT_FALSE(ParseMimeTypes(TempFile(), map));
}

// xxd /tmp/mimetest/mime.cache
// 00000000: 0001 0002 0000 0074 0000 0078 0000 007c  .......t...x...|
// 00000010: 0000 008c 0000 01cc 0000 01d0 0000 01dc  ................
// 00000020: 0000 01e0 0000 01e4 0000 01e8 6170 706c  ............appl
// 00000030: 6963 6174 696f 6e2f 7064 6600 782f 736d  ication/pdf.x/sm
// 00000040: 696c 6500 782f 6967 6e6f 7265 0000 0000  ile.x/ignore....
// 00000050: 7465 7874 2f70 6c61 696e 0000 782f 666f  text/plain..x/fo
// 00000060: 6f00 0000 7e00 0000 782f 6e6f 2d64 6f74  o...~...x/no-dot
// 00000070: 0000 0000 0000 0000 0000 0000 0000 0001  ................
// 00000080: 0000 0064 0000 0068 0000 0032 0000 0005  ...d...h...2....
// 00000090: 0000 0094 0000 0063 0000 0001 0000 00d0  .......c........
// 000000a0: 0000 0066 0000 0001 0000 00dc 0000 006f  ...f...........o
// 000000b0: 0000 0001 0000 00e8 0000 0074 0000 0001  ...........t....
// 000000c0: 0000 00f4 0001 f929 0000 0001 0000 0100  .......)........
// 000000d0: 0000 006f 0000 0001 0000 010c 0000 0064  ...o...........d
// 000000e0: 0000 0001 0000 0118 0000 006f 0000 0001  ...........o....
// 000000f0: 0000 0124 0000 0078 0000 0001 0000 0130  ...$...x.......0
// 00000100: 0001 f642 0000 0001 0000 013c 0000 0064  ...B.......<...d
// 00000110: 0000 0001 0000 0148 0000 0070 0000 0001  .......H...p....
// 00000120: 0000 0154 0000 0066 0000 0001 0000 0160  ...T...f.......`
// 00000130: 0000 0074 0000 0001 0000 016c 0000 002e  ...t.......l....
// 00000140: 0000 0001 0000 0178 0000 002e 0000 0001  .......x........
// 00000150: 0000 0184 0000 002e 0000 0001 0000 0190  ................
// 00000160: 0000 002e 0000 0003 0000 019c 0000 002e  ................
// 00000170: 0000 0001 0000 01c0 0000 0000 0000 003c  ...............<
// 00000180: 0000 0032 0000 0000 0000 0050 0000 0032  ...2.......P...2
// 00000190: 0000 0000 0000 002c 0000 0032 0000 0000  .......,...2....
// 000001a0: 0000 0044 0000 003c 0000 0000 0000 005c  ...D...<.......\
// 000001b0: 0000 0050 0000 0000 0000 0050 0000 0032  ...P.......P...2
// 000001c0: 0000 0000 0000 0050 0000 0032 0000 0000  .......P...2....
// 000001d0: 0000 0000 0000 0000 0000 01dc 0000 0000  ................
// 000001e0: 0000 0000 0000 0000 0000 0006 0000 0000  ................
// 000001f0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
// 00000200: 0000 0000
TEST_F(ParseMimeTypesTest, Invalid) {
  auto buf = Base64Decode(kTestMimeCacheB64);
  ASSERT_TRUE(buf.has_value());
  // ALIAS_LIST_OFFSET is uint32 at byte 4 = 0x74.
  // Alias list offset inside header.
  InvalidIf(*buf, 7, 0xa);
  // Alias list offset larger than file size.
  InvalidIf(*buf, 6, 0xff);
  // Not null beore alias list.
  InvalidIf(*buf, 0x74 - 1, 'X');
  // Misaligned offset for REVERSE_SUFFIX_TREE_OFFSET.
  InvalidIf(*buf, 0x13, 0x7a);
  // N_ROOTS > kMaxUnicode (0x10ffff).
  InvalidIf(*buf, 0x8d, 0x20);
  InvalidIf(*buf, 0xd5, 0x20);
  // Node C > kMaxUnicode (0x10ffff).
  InvalidIf(*buf, 0x95, 0x20);
  // Node N_CHILDREN > kMaxUnicode (0x10ffff).
  InvalidIf(*buf, 0x99, 0x20);
  // Node FIRST_CHILD_OFFSET below tree offset.
  InvalidIf(*buf, 0x9f, 0x10);
  InvalidIf(*buf, 0xdb, 0x20);
  // Node FIRST_CHILD_OFFSET beyond file size.
  InvalidIf(*buf, 0x9e, 0x20);
  InvalidIf(*buf, 0xda, 0x20);
  // Mime type offset below header.
  InvalidIf(*buf, 0x18b, 0x10);
  // Mime type offset above alias list.
  InvalidIf(*buf, 0x18b, 0x74);
}

}  // namespace base::nix
