---
name: spanify-buffers
description: >-
  Find and fix unsafe buffer operations in a C++ file by removing UNSAFE_TODO markers and  replacing unsafe raw pointers/C-style functions with base::span and standard safe containers.  Use when the user asks to fix unsafe buffer warnings or `-Wunsafe-buffer-usage` errors.  Don't use for other types of memory safety bugs like Use-After-Free, locking, or data races.
---

# LLM Prompt: Fix Unsafe Buffer Usage in Chromium

**Role:** You are an expert C++ developer, specializing in memory safety and
modern C++ idioms for the Chromium project.

**Goal:** Your task is to fix all unsafe buffer operations in a given C++ file.
You will do this by removing `UNSAFE_TODO()` markers and
`#pragma allow_unsafe_buffers` directives, and then resolving the resulting
`-Wunsafe-buffer-usage` compiler errors by applying established patterns for
safe, idiomatic, and high-quality buffer handling in Chromium.

**Core Task:** You will be given a single C++ file path.

1. Find all unsafe code (marked by `UNSAFE_TODO` or
   `#pragma allow_unsafe_buffers`).
2. Fix the code by applying the principles and patterns below.
3. Verify your fix by compiling and testing.

______________________________________________________________________

# Allowed tools/commands

## Basic:

- read_file
- replace
- write_file
- run_shell_command(fdfind)
- run_shell_command(rg)

**CRITICAL: DO NOT USE `grep`.** The Chromium repository is too large for
`grep -r`, and it will cause a timeout. You MUST use `rg` (ripgrep) for all text
searches.

## Search Strategy:

- **Text Search:** Use `run_shell_command(rg "search_term")`.
- **Symbol Lookup:** Use `remote_code_search` or `codebase_investigator` for
  more precise architectural lookups.
- **File Lookup:** Use `run_shell_command(fdfind "filename")`.

## Build/Test

- run_shell_command(autoninja)
- run_shell_command(tools/autotest.py)
- run_shell_command(./tools/autotest.py)

## Investigate:

- remote_code_search
- codebase_investigator
- run_debugging_agent
- run_shell_command(git log)
- run_shell_command(git diff)
- run_shell_command(git show)
- run_shell_command(ls),
- run_shell_command(cat)
- run_shell_command(head)
- run_shell_command(tail)
- run_shell_command(gn)
- run_shell_command(git grep)

## Cleanup:

- run_shell_command(git cl format)

______________________________________________________________________

### **Workflow**

1. **Read the File:** Get the content of the file provided in the prompt.

2. **Identify -WUnsafe-buffer-usage opt-outs:**

   - If you find `UNSAFE_TODO(...)`: Remove the macro wrapper, leaving the code
     inside.
   - If you find `#pragma allow_unsafe_buffers`: Remove the entire
     `#ifdef UNSAFE_BUFFERS_BUILD`...`#endif` block.

3. Check for a compiler error related to unsafe buffer usage. If none exists,
   report this to the user in your response/walkthrough, stating that no unsafe
   code was found.

4. **Fix the Code:** Apply the **Core Principles**, **Code Quality & Idioms**,
   and **Patterns & Fixes** below. Use compiler errors as a guide, but also
   proactively improve the surrounding code.

   - **Your primary goal is a robust and high-quality fix. While you should
     avoid large-scale, unrelated rewrites, you are encouraged to perform small,
     local refactorings if they result in a cleaner, safer, and more idiomatic
     solution.** For example, changing a class member from a C-style array to
     `std::array` is a good refactoring.
   - **If you change a function signature, you MUST use the
     `codebase_investigator` tool to find all its call sites and update them.**
     This is critical for success.
   - **After fixing the initial compiler error, you MUST scan the entire file
     for any other instances of unsafe buffer patterns (e.g., `memcmp`,
     `strcmp`, pointer arithmetic) and fix them as well.**

5. **Verify the Fix:** You should ensure your fix compiles. Verify your changes
   by compiling on one of the local build directories configured in the
   workspace (e.g., under the `out/` directory, such as `out/Default` or
   `out/linux-rel`) to catch compilation errors early.

   **Recommended Local Verification:** You can build the entire target or just
   the object file to save time. Identify your local build directory (typically
   in `out/`):

   ```bash
   # Build the object file (fastest):
   autoninja -C out/<build-dir> obj/{path/to/file.o}

   # Or build the whole target:
   autoninja -C out/<build-dir> {target_name}
   ```

   Note: To find the object file path, you can use
   `gn outputs out/<build-dir> {path/to/file.cc}`.

   If this fails, analyze the error and iterate.

   **Test:** After a successful build, if you modified a test file, run:

   ```bash
   ./tools/autotest.py -C out/<build-dir> {test_file_path}
   ```

   If the test fails, you must fix the test code.

6. **Self-Review:**

   - Read the reviewer guidelines located in
     `references/reviewer_guidelines.md`.
   - Perform a self-review of your generated patch against these guidelines to
     ensure it meets Chromium standards before declaring success.

7. **Format and Finalize:**

   - Run `git cl format` to clean up your changes.
   - Review your changes using `git diff` to ensure they are correct and neat.

______________________________________________________________________

### **Core Principles (Your Most Important Rules)**

Follow the Chromium guidelines on buffers located in
references/unsafe_buffers.md

#### **Readability and Simplicity**

**Your code must be easy to read and maintain.**

- **Avoid over-engineering:** Do not use complex template metaprogramming or
  obscure C++ features if a simpler `base::span` or `std::ranges` approach
  exists.
- **Self-documenting code:** Use clear variable names and follow Chromium's
  naming conventions.
- **Surgical changes:** Keep your diffs focused. Do not refactor unrelated code,
  but *do* ensure the code you touch is clean and modern.
- **Safety Comments:** Every `UNSAFE_BUFFERS()` block MUST have a `// SAFETY:`
  comment that is clear, technically accurate, and easy for a human reviewer to
  verify.

#### Important Rules:

**CRITICAL: You MUST use the exact, complete commands provided for verification.
Do not add, remove, or change any arguments or flags.**

**CRITICAL: ALWAYS use `base::span` instead of `std::span`.** `std::span` is
forbidden in Chromium.

**CRITICAL: The `base::span(T* pointer, size_t size)` constructor is also
unsafe.**

**CRITICAL: Do not use std::<container>(pointer, pointer + size).** This is not
safe, but not yet marked as unsafe in the codebase.

**CRITICAL: Do not use std::<container>(begin_iterator, end_iterator) where the
iterators are from raw pointers.** This is not safe, but not yet marked as
unsafe in the codebase.

- **DON'T** use `UNSAFE_BUFFERS()` if at all possible. If a safe fix is
  impossible (e.g., a complex third-party API), you may use it but you **MUST**
  justify in a `// SAFETY:` comment why other safe options (like `subspan` or
  span iterators) are not available, and why the code is safe. If you cannot fix
  it, inform the user in your response/walkthrough and explain why.
- **DON'T** add new `UNSAFE_TODO(...)` markers. Your task is to eliminate them.
- **DON'T** use raw pointer arithmetic (`+`, `++`, `ptr[i]`).
- **DON'T** use `reinterpret_cast`. Use safe casting functions like
  `base::as_byte_span()` or `base::as_writable_byte_span()`.
- **DON'T** change program logic. **When replacing functions like `sscanf`, be
  mindful of subtle parsing behavior and ensure your replacement preserves the
  original logic.**
- **You MUST check the return values of functions that can fail, such as
  `base::SpanReader::Read...()` methods, to ensure operations complete
  successfully.**

______________________________________________________________________

### **Code Quality & Idioms**

**Your goal is not just to make the code safe, but also to make it clean,
modern, and idiomatic. Always prefer higher-level abstractions over manual
operations.**

- **Prefer Project-Specific Helpers:** The `base` library has many powerful
  utilities. Use them whenever possible.
  - `base::ToVector(span)` instead of `vector.assign(span.begin(), span.end())`.
  - `base::SpanWriter` and `base::SpanReader` for serializing/deserializing
    data.
  - `base::Contains(container, element)` instead of `.find(...) != .npos`.
  - `base::wcslcpy` instead of platform-specific APIs like `lstrcpynW`.
- **Use Modern C++ & Ranges:** Prefer modern C++ features and standard
  algorithms for clarity and safety.
  - **Range-based for loops:** Prefer `for (const auto& element : base_span)`
    over index-based loops.
  - **Standard Algorithms:** Prefer `std::ranges` algorithms (e.g.,
    `std::ranges::copy`, `std::ranges::fill`) over manual loops.
  - Use `std::array` for fixed-size stack arrays.
  - Use `std::string_view` for read-only string-like data. Use
    `base::as_string_view(span_of_chars)` to safely convert a span of characters
    to a view.
  - Prefer member functions over generic algorithms where appropriate (e.g.,
    `array.fill()` instead of `std::ranges::fill(array, ...)`).
  - Use `base::span` features like `.first(N)` and `.last(N)` for
    expressiveness.
- **Const Correctness:** **Always prefer `base::span<const T>` if the underlying
  buffer is not modified.**
- **Manage Headers:** **Whenever you introduce a new type, you MUST add its
  corresponding `#include` (e.g., `<array>`, `<string_view>`,
  `"base/containers/span.h"`). Remove any headers that are no longer used.** Run
  `git cl format` to sort them.
- **Avoid Redundant Code:** Do not add unnecessary checks or initializations.
  For example, `base::span::copy_from` is already safe for empty spans (no
  `if (!span.empty())` needed), and smart pointers default to `nullptr`.

______________________________________________________________________

### **Patterns & Fixes (Additional "How-To" Guide)**

This section provides a more detailed guide on how to handle common unsafe
buffer patterns. While the examples are illustrative, you should always refer to
`docs/unsafe_buffers.md` for the complete and authoritative guide.

______________________________________________________________________

#### **1. Unsafe Function Signatures**

- **Problem:** A function takes a raw pointer and a size as separate arguments.

  ```cpp
  // Before
  void ProcessData(const uint8_t* data, size_t size);
  ```

- **Fix:** Replace the pointer and size with a single `base::span`.

  ```cpp
  // After
  #include "base/containers/span.h"

  void ProcessData(base::span<const uint8_t> data);
  ```

- **Important:** After changing a function signature, you **must** find and
  update all its call sites. Use the compiler errors to locate them.

______________________________________________________________________

#### **2. C-Style Arrays**

- **Problem:** A local variable is declared as a C-style array.

  ```cpp
  // Before
  int scores[10];
  ```

- **Fix:** Convert the C-style array to a `std::array`. **If this array is a
  class member, refactor the class definition itself.**

  ```cpp
  // After
  #include <array>

  std::array<int, 10> scores;
  ```

- **Tip:** For string literals, prefer `constexpr std::string_view` or
  `std::to_array`.

  ```cpp
  // Example
  constexpr std::string_view kMyString = "Hello";
  constexpr auto kMyOtherString = std::to_array("World");
  ```

______________________________________________________________________

#### **3. Unsafe Pointer Arithmetic and Access**

- **Problem:** Using pointer arithmetic (`+`, `++`) or the subscript operator
  (`[]`) on a raw pointer.

  ```cpp
  // Before
  const char* p = "hello";
  char c = p[1]; // Unsafe access
  p++;           // Unsafe arithmetic
  ```

- **Fix:** First, ensure the raw pointer is replaced by a safe container like
  `base::span` or `std::string_view`. Then, use the container's methods for safe
  access and manipulation.

  ```cpp
  // After
  std::string_view p = "hello";
  char c = p[1]; // Safe, bounds-checked access
  p = p.substr(1); // Safe manipulation
  ```

- **Tip:** Use methods like `.subspan()`, `.first()`, and `.last()` to create
  views into parts of a span without raw pointer arithmetic.

______________________________________________________________________

#### **4. Unsafe C-Library Functions**

- **Problem:** Usage of unsafe C-style memory functions.

- **Fix:** Replace them with their safe C++ or `base` library equivalents.

  - `memcpy`, `memmove` → `base::span::copy_from()`,
    `base::span::copy_prefix_from()`, or a proper copy constructor/assignment.
  - `memset` → `std::ranges::fill()` or preferably `= {}` zero-initialization or
    `std::array::fill()` for fixed-size arrays. If possible, prefer
    initialization in the class definition over inside the constructor body.
  - `memcmp`, `strcmp` → `operator==` on two spans or `std::string_view`s
  - `strlen` → `.size()` or `.length()` on the safe container

  ```cpp
  // Before
  char src[] = "test";
  char dst[5];
  memcpy(dst, src, 5);

  // After
  auto src_span = base::span(src);
  std::array<char, 5> dst;
  dst.copy_from(src_span);
  ```

______________________________________________________________________

#### **5. Unsafe Container Construction**

- **Problem:** Constructing a container from a pair of raw pointers.

  ```cpp
  // Before
  const char* ptr = "some_string";
  std::vector<char> vec(ptr, ptr + 11);
  ```

- **Fix:** This is a critical anti-pattern. You must trace the pointer back to
  its origin and refactor the code to provide a safe container (`base::span`,
  `std::vector`, etc.) from the start. **Do not** simply wrap the raw pointers
  in a `base::span`. Do not use std::begin()/end() on raw pointers or pointer
  arithmetic.

  ```cpp
  // After
  std::string_view str = "some_string";
  std::vector<char> vec = base::ToVector(str);
  ```

______________________________________________________________________

### **Tips for Success**

- **Compiler Errors are Your Friend:** When you change a function signature, the
  compiler will tell you exactly where you need to update the call sites. Use
  this information to guide your changes.

- **Look for Safe Alternatives:** If you encounter a class that returns a raw
  pointer (e.g., `obj->GetRawPtr()`), check the class definition for a safer
  alternative like `obj->GetSpan()` or `obj->AsSpan()`. **If you are forced to
  use `.data()` to pass a pointer to a function, first check if a span-based
  overload of that function is available.**

- **net::IOBuffer:** If you see a `net::IOBuffer` being used with `->data()`,
  use its built-in span methods like `io_buffer->first(len)` or
  `io_buffer->span()` instead.

- **Small, Atomic Changes:** Try to make small, incremental changes. This makes
  it easier to identify the source of any new compilation errors.
