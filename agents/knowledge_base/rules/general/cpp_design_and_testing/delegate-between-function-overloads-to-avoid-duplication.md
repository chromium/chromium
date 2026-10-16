---
name: general/delegate-between-function-overloads-to-avoid-duplication
rubric: design
severity: should_fix
---

**Rule.** When introducing or maintaining function overloads (such as
pointer-and-length parameters alongside span/string_view), convenience wrappers,
or notifying and non-notifying method variants (such as \*NoNotification or
\*Silently), avoid duplicating implementation logic. Implement notifying
methods, wrappers, and overloads to delegate directly to the non-notifying or
canonical implementation (or a shared helper) before dispatching notifications
or returning.

**Why.** Duplicating validation checks, state mutations, decoding, or iteration
logic across overloads and wrapper variants increases maintenance overhead and
risks logic drift or incomplete bug fixes. Delegating to a canonical or
non-notifying implementation centralizes behavior and ensures consistent state
transitions.

## Examples

Bad:

```cpp
// 1. Duplicating logic across pointer/length and span overloads:
bool ReadUnicodeCharacter(const char* src,
                          size_t src_len,
                          size_t* char_index,
                          base_icu::UChar32* code_point_out) {
  base_icu::UChar32 code_point;
  UNSAFE_TODO(CBU8_NEXT(reinterpret_cast<const uint8_t*>(src), *char_index,
                        src_len, code_point));
  *code_point_out = code_point;
  (*char_index)--;
  return IsValidCodepoint(code_point);
}

// 2. Duplicating validation and state mutations in notifying method variants:
void MemoryConsumer::UpdateMemoryLimit(int percentage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(percentage, 0);
  memory_limit_ = percentage;
  OnUpdateMemoryLimit();
}

void MemoryConsumer::UpdateMemoryLimitNoNotification(int percentage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(percentage, 0);
  memory_limit_ = percentage;
}
```

Good:

```cpp
// 1. Delegating from legacy pointer/length overload to safe view overload:
bool ReadUnicodeCharacter(const char* src,
                          size_t src_len,
                          size_t* char_index,
                          base_icu::UChar32* code_point_out) {
  return ReadUnicodeCharacter(std::string_view(src, src_len), char_index,
                              code_point_out);
}

// 2. Delegating notifying variant to non-notifying helper before notification:
void MemoryConsumer::UpdateMemoryLimit(int percentage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateMemoryLimitNoNotification(percentage);
  OnUpdateMemoryLimit();
}

void MemoryConsumer::UpdateMemoryLimitNoNotification(int percentage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(percentage, 0);
  memory_limit_ = percentage;
}
```
