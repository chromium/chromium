// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "cc/paint/paint_op_buffer.h"
#include "cc/test/paint_op_helper.h"
#include "cc/test/test_options_provider.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"

// paint_op_buffer_eq_fuzzer deserializes and reserializes paint ops to
// make sure that this does not modify or incorrectly serialize them.
// This is intended to be a fuzzing correctness test.
//
// Compare this to paint_op_buffer_fuzzer which makes sure that deserializing
// ops and rasterizing those ops is safe.
//
// This test performs the following operation:
//
// serialized1 -> deserialized1 -> serialized2 -> deserialized2 -> serialized3
//
// Ideally this test would compare serialized1 to serialized2, however:
// (1) Deserializing is a destructive process on bad input, e.g. SkMatrix
//     that says it is identity will be clobbered to have identity values.
// (2) Padding for alignment is skipped and so serialized1 may have garbage.
//     serialized2 and serialized3 are cleared to zero first.
// (3) Any internal allocated memory (e.g. DrawRecord ops) also will not
//     be initialized to zero, and some ops memcpy when serializing or
//     deserializing, and may copy some of this garbage.
//
// It'd be nice to be able to binary compare, but because of all the above
// reasons, this is impossible to do for all ops, so this test only does
// a logical comparison of deserialized1 and deserialized2, and verifies
// that serialized2 and serialized3 wrote the exact same number of bytes.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const size_t kMaxSerializedSize = 1000000;

  SkNoDrawCanvas canvas(100, 100);
  cc::TestOptionsProvider test_options_provider;

  // This test is trying to verify that serializing and deserializing doesn't
  // change the results.  Scaling paint shaders and changed them to a fixed
  // scale when serializing defeats this purpose, so turn that off.
  test_options_provider.mutable_serialize_options().scale_paint_record_shaders =
      false;

  // Need 4 bytes to be able to read the type/skip.
  if (size < 4)
    return 0;

  size_t serialized_size = reinterpret_cast<const cc::PaintOp*>(data)->skip;
  if (serialized_size > kMaxSerializedSize)
    return 0;

  // If the op has a skip that runs off the end of the input, then ignore.
  if (serialized_size > size)
    return 0;

  std::unique_ptr<char, base::AlignedFreeDeleter> deserialized1(
      static_cast<char*>(base::AlignedAlloc(sizeof(cc::LargestPaintOp),
                                            cc::PaintOpBuffer::PaintOpAlign)));
  size_t bytes_read1 = 0;
  cc::PaintOp* deserialized_op1 = cc::PaintOp::Deserialize(
      data, size, deserialized1.get(), sizeof(cc::LargestPaintOp), &bytes_read1,
      test_options_provider.deserialize_options());

  // Failed to deserialize, so abort.
  if (!deserialized_op1)
    return 0;

  // DrawTextBlob ops contain two pieces of information: a text blob, and a
  // vector of typefaces that are used in the blob. However, during
  // deserialization we never need to reconstruct the vector of typefaces, since
  // it is only used to reconstruct the blob directly. This, however, poses a
  // problem for this fuzzer since we will then try to serialize the op again,
  // resulting in an assert. The assert says that we don't have typefaces
  // (serialized from the vector) that are required to serialize the text blob.
  // The solution is to skip the equality fuzzer for DrawTextBlob ops.
  //
  // Normally the initial deserialization would also fail and we would early out
  // above, since the transfer cache doesn't have any entries to reconstruct the
  // text blob. However, Skia, given some data, seems to be able to construct an
  // SkTextBlob without accessing the given cataloger. See crbug.com/798939 for
  // an example of such a test case.
  if (deserialized_op1->GetType() == cc::PaintOpType::DrawTextBlob) {
    deserialized_op1->DestroyThis();
    return 0;
  }

  // If we get to this point, then the op should be ok to serialize/deserialize
  // and any failure is a dcheck.
  std::unique_ptr<char, base::AlignedFreeDeleter> serialized2(
      static_cast<char*>(base::AlignedAlloc(serialized_size,
                                            cc::PaintOpBuffer::PaintOpAlign)));
  memset(serialized2.get(), 0, serialized_size);
  size_t written_bytes2 =
      deserialized_op1->Serialize(serialized2.get(), serialized_size,
                                  test_options_provider.serialize_options());
  CHECK_GT(written_bytes2, 0u);
  CHECK_LE(written_bytes2, serialized_size);

  std::unique_ptr<char, base::AlignedFreeDeleter> deserialized2(
      static_cast<char*>(base::AlignedAlloc(sizeof(cc::LargestPaintOp),
                                            cc::PaintOpBuffer::PaintOpAlign)));
  size_t bytes_read2 = 0;
  cc::PaintOp* deserialized_op2 = cc::PaintOp::Deserialize(
      data, size, deserialized2.get(), sizeof(cc::LargestPaintOp), &bytes_read2,
      test_options_provider.deserialize_options());
  CHECK(deserialized_op2);
  CHECK_EQ(bytes_read1, bytes_read2);

  std::unique_ptr<char, base::AlignedFreeDeleter> serialized3(
      static_cast<char*>(
          base::AlignedAlloc(written_bytes2, cc::PaintOpBuffer::PaintOpAlign)));
  memset(serialized3.get(), 0, written_bytes2);
  size_t written_bytes3 =
      deserialized_op2->Serialize(serialized3.get(), written_bytes2,
                                  test_options_provider.serialize_options());
  CHECK_EQ(written_bytes2, written_bytes3);

  CHECK(*deserialized_op1 == *deserialized_op2)
      << "\n1: " << cc::PaintOpHelper::ToString(deserialized_op1)
      << "\n2: " << cc::PaintOpHelper::ToString(deserialized_op2);

  deserialized_op1->DestroyThis();
  deserialized_op2->DestroyThis();

  return 0;
}
