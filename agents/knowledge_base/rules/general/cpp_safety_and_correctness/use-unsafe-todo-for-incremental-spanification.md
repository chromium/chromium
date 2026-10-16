---
name: general/use-unsafe-todo-for-incremental-spanification
rubric: safety
severity: should_fix
---

**Rule.** Do not add or retain file-level `#pragma allow_unsafe_buffers`; remove
the pragma and modernize code to safe containers or localize opt-outs to minimal
expressions. When temporarily suppressing unconverted buffer operations during
incremental spanification, wrap expressions in `UNSAFE_TODO(...)` without a
`// SAFETY:` comment. Reserve `UNSAFE_BUFFERS(...)` (accompanied by a required
`// SAFETY:` comment) strictly for permanent, audited exemptions such as
external C-style APIs or `argv` indexing.

**Why.** File-wide pragmas disable -Wunsafe-buffer-usage across the entire
translation unit, hiding buffer safety defects in surrounding functions.
Localizing exemptions keeps compiler checks active, while distinguishing
permanent audited exemptions (UNSAFE_BUFFERS with // SAFETY:) from temporary
technical debt (UNSAFE_TODO without comments) ensures automated spanification
tools and reviewers can track remaining debt.

## Examples

Bad:

```cpp
// Bad: Opting out the entire file with a pragma.
#ifdef UNSAFE_BUFFERS_BUILD
#pragma allow_unsafe_buffers
#endif

// Bad: Attaching SAFETY comment to temporary migration debt.
// SAFETY: Temporary suppression for incremental migration.
UNSAFE_BUFFERS(CBU16_APPEND_UNSAFE(&(*output)[0], char_offset, code_point));
```

Good:

```cpp
// Good: Localized, permanent exemption with SAFETY justification.
// SAFETY: Standard C++ main contract guarantees argv has argc valid elements.
auto args = UNSAFE_BUFFERS(base::span(argv, static_cast<size_t>(argc)));

// Good: Temporary migration debt marked with UNSAFE_TODO (no SAFETY comment).
UNSAFE_TODO(CBU16_APPEND_UNSAFE(&(*output)[0], char_offset, code_point));
```
