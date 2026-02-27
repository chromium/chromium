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
4. Generate the required output files.

______________________________________________________________________

# Allowed tools/commands

## Basic:

- read_file
- replace
- write_file
- run_shell_command(fdfind)
- run_shell_command(rg)

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
   report `UNSAFE_TODO` in the output JSON with a summary stating that no unsafe
   code was found. You need to build all the builders from step 5 to confirm
   this.

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

5. **Verify the Fix:** You must ensure your fix compiles. **This step is
   mandatory.**

   You will run the exact verification commands below for each of the builders.

   **Linux:**

   ```
   autoninja -C out/linux-rel --quiet
   ```

   **Windows:**

   ```
   autoninja -C out/linux-win-cross-rel --quiet
   ```

   **Android:**

   ```
   autoninja -C out/android-14-x64-rel --quiet
   ```

   **Mac:**

   ```
   autoninja -C out/mac-rel --quiet
   ```

   **ChromeOS**

   ```
   autoninja -C out/linux-chromeos-rel --quiet
   ```

   **Iterate:** If any command fails for any builder, **you must analyze the
   error and try a different fix.** Do not proceed until all commands pass for
   all builders.

   **Test:** After a successful build, if you modified a test file, select the
   appropriate builder and run:

   ```
   ./tools/autotest.py ./out/{builder_name} {test_file_path}
   ```

   If the test fails, you must fix the test code.

6. **Format and Finalize:**

   - Run `git cl format` to clean up your changes.
   - Generate the output files as specified below:

   1. **`gemini_out/summary.json`:** A JSON file with the result.

   - **On success:**
     ```json
     {
       "status": "SUCCESS",
       "summary": "Successfully spanified the file by replacing [Problem] with [Solution]."
     }
     ```
   - **If compilation fails:**
     ```json
     {
       "status": "COMPILE_FAILED",
       "summary": "Attempted to fix [Problem] but failed to compile with error: [Copy compiler error here]."
     }
     ```
   - **If fix is impossible:**
     ```json
     {
       "status": "UNSAFE_TODO",
       "summary": "Cannot fix unsafe usage due to [Reason, e.g., complex third-party API]."
     }
     ```

   2. **`gemini_out/commit_message.md`:** A commit message for the change.

   ```markdown
   Fix unsafe buffer usage in [filename or class]

   Replaced [brief summary of change, e.g., raw pointer parameters with base::span]
   to fix unsafe buffer error(s).

   Initial patchset generated by headless gemini-cli using:
   //agents/prompts/projects/spanification/run.py
   ```

   - The commit message should be concise but informative.
   - The text width should not exceed 72 characters per line.
   - The header line should be 50 characters or less. You can transform the file
     path by removing directory components or take the relevant class name.

7. **Final step:** Check the files exist:

   - `gemini_out/summary.json`
   - `gemini_out/commit_message.md`

______________________________________________________________________

### **Core Principles (Your Most Important Rules)**

Follow the content of @unsafe_buffers.md

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

- **DON'T** use `UNSAFE_BUFFERS()`. If a safe fix is impossible (e.g., a complex
  third-party API), set the status to `UNSAFE_TODO` in `summary.json` and stop
  without creating a `commit_message.md`.
- **DON'T** add new `UNSAFE_TODO(...)` or `UNSAFE_BUFFERS(...)` markers. Your
  task is to eliminate them.
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
