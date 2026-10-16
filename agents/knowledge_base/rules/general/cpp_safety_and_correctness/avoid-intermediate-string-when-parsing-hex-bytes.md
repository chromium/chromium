---
name: general/avoid-intermediate-string-when-parsing-hex-bytes
rubric: safety
severity: should_fix
---

**Rule.** Do not construct, append to, or reset intermediate std::string
accumulators in loops to parse hexadecimal strings into byte arrays or spans.
Decode hex digit pairs directly into the destination buffer or span using
base::HexDigitToInt(), (HexDigitToInt(hi) \<< 4) | HexDigitToInt(lo), or
base::HexStringToSpan() / base::HexStringToBytes().

**Why.** Allocating, appending to, and clearing temporary std::string instances
inside parsing loops incurs heap allocations, buffer copies, and redundant
string-to-integer conversion overhead. Direct digit decoding or span-based
conversion processes characters in-place with zero allocations and optimal
performance.

## Examples

Bad:

```cpp
std::string current_byte_string;
for (size_t i = 0; i < input.length(); ++i) {
  current_byte_string.push_back(base::ToLowerASCII(input[i]));
  if (current_byte_string.size() == 2u) {
    uint32_t out_converted_hex = 0;
    base::HexStringToUInt(current_byte_string, &out_converted_hex);
    target_bytes[byte_index++] = static_cast<uint8_t>(out_converted_hex);
    current_byte_string.clear();
  }
}
```

Good:

```cpp
// Option 1: Direct digit decoding in a loop.
for (size_t i = 0; i < input.size(); i += 2, ++byte_index) {
  target_bytes[byte_index] = static_cast<uint8_t>(
      (base::HexDigitToInt(input[i]) << 4) | base::HexDigitToInt(input[i + 1]));
}

// Option 2: Bulk span decoding for contiguous hex strings.
if (!base::HexStringToSpan(input, target_bytes)) {
  return false;
}
```
