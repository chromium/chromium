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

3. **Verify Initial Compiler Error:** Check for a compiler error related to
   unsafe buffer usage. If none exists, report `UNSAFE_TODO` in the output JSON
   with a summary stating that no unsafe code was found. You need to build all
   the builders from step 6 to confirm this.

4. **Analyze, Categorize, and Plan:**
   Before modifying or writing any code, perform a formal analysis and planning
   phase. Modern Chromium memory safety is not just about silencing warnings
   mechanically; it is about designing robust, clean C++ code. Reviewers will
   reject lazy pointer-to-span wrapping at call sites. Follow this plan:

   - **A. Variable Context Categorization:** Trace variables to understand their
     lifecycle:
     - *Already-Safe:* The underlying container is already bounds-checked (e.g.,
       `std::vector`, `std::array`, `base::HeapArray`, or `base::span`). Simply
       remove the `UNSAFE_TODO` or legacy pointer accesses (`.data()`,
       `&vec[0]`).
     - *Local-Variable:* The unsafe pointer or raw array is restricted to a
       single function block.
     - *Method-Argument:* The raw pointer or size parameter is part of a
       function/method signature. This requires a *Cascading Signature Migration*
       (see step 4-D).
   - **B. Unsafe Pattern Cluster Classification:** Classify the warning into a
     specific cluster (`operator[]` on raw pointer, `Pointer-Arithmetic`,
     `Safe-Container-Construction`, or `Unsafe-Std-Function`) to choose the
     correct refactoring pattern.
   - **C. Architectural Preferences:** Do NOT perform a mechanical pointer-to-span
     wrapping (e.g., wrapping `ptr` in `base::span(ptr, size)`) if you can instead
     improve the design:
     - Prefer safe containers like `std::array` (relying on CTAD for array bounds
       deduction, e.g. `std::array arr = { ... }`) or `std::vector`/`base::HeapArray`.
     - Trace to the source and refactor functions to return a safe container or
       `base::span` directly.
     - Avoid double wrapping (do not construct a `base::span` from another span).
   - **D. Cascading Signature Migration Planning:** If a method signature changes
     to accept `base::span`:
     1. Update both the header (`.h`) and implementation (`.cc`) files.
     2. Use search tools (like `codebase_investigator`) to locate all call sites
        and recursively migrate them.
     3. Trace all virtual overrides, subclass implementations, and interface
        declarations in class hierarchies and update them to match.

5. **Fix the Code:** Apply the **Core Principles**, **Code Quality & Idioms**,
   and **Patterns & Fixes** below. Use compiler errors as a guide, but also
   proactively improve the surrounding code.
   - **Your primary goal is a robust and high-quality fix. While you should
     avoid large-scale, unrelated rewrites, you are encouraged to perform small,
     local refactorings if they result in a cleaner, safer, and more idiomatic
     solution.**
   - **After fixing the initial compiler error, you MUST scan the entire file
     for any other instances of unsafe buffer patterns (e.g., `memcmp`,
     `strcmp`, pointer arithmetic) and fix them as well.**

6. **Verify the Fix:** Ensure your changes compile. It is highly recommended to
   verify your changes on at least one local builder (typically Linux) to catch obvious
   errors early.
   - **Local Verification (Linux):** Build the object file or the full target:
     ```bash
     # Build the object file (fastest):
     autoninja -C out/linux-rel obj/{path/to/file.o}

     # Or build the whole target:
     autoninja -C out/linux-rel {target_name}
     ```
     (Use `gn outputs out/linux-rel {path/to/file.cc}` to find the object file path).
   - **Test:** If you modified a test file, run:
     ```bash
     ./tools/autotest.py ./out/linux-rel {test_file_path}
     ```
     If the test fails, you must fix the test code.

7. **Format, Self-Review, and Finalize:**
   - Run `git cl format` to clean up your changes.
   - Run a self-review of your changes against the reviewer checklist:
     1. **Header Cleanliness & Legality:** Verify you strictly followed the
        `base::span` guidelines (no `std::span` or `<span>` imports). Clean up
        obsolete header imports (like `<string.h>` or `<cstring>`) if legacy
        functions were removed.
     2. **Wrap Check:** Verify you did not introduce redundant double-wrapping.
     3. **Naming Style:** If you migrated method arguments, ensure variable names
        are adjusted to modern C++ style (e.g. renaming `data_ptr` to `data`).
     4. **SAFETY Comments Check:** Verify that any remaining `UNSAFE_BUFFERS()`
        blocks strictly comply with the `// SAFETY:` comment guidelines.
   - Generate the output files:
     1. **`gemini_out/summary.json`:**
        - On success: `{"status": "SUCCESS", "summary": "..."}`
        - On compilation failure: `{"status": "COMPILE_FAILED", "summary": "..."}`
        - If fix is impossible: `{"status": "UNSAFE_TODO", "summary": "..."}`
     2. **`gemini_out/commit_message.md`:** A commit message for the change (text
        width <= 72 chars, header line <= 50 chars).

8. **Final Step:** Verify both output files exist:
   - `gemini_out/summary.json`
   - `gemini_out/commit_message.md`

### **Core Principles (Your Most Important Rules)**

Follow the content of @unsafe_buffers.md

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

#### **6. Already-Safe Context & operator[]**

- **Problem:** Unnecessary UNSAFE_TODO/UNSAFE_BUFFERS wrapping around containers
  that are already bounds-checked and safe.

  ```cpp
  // Before
  int val = UNSAFE_TODO(my_vector[index]);
  ```

- **Fix:** If the underlying container is already a bounds-checked type (e.g.,
  `std::array`, `std::vector`, `base::HeapArray`, or a `base::span`), simply
  remove the unsafe macro wrapper.

  ```cpp
  // After
  int val = my_vector[index];
  ```

______________________________________________________________________

#### **7. Already-Safe + memcmp / strcmp Double Wrapping**

- **Problem:** Using memcmp on safe containers or raw pointer conversions for
  comparisons.

  ```cpp
  // Before
  bool is_equal = UNSAFE_TODO(memcmp(array1.data(), array2.data(),
                                     array1.size()) == 0);
  ```

- **Fix:** Replace C-style comparisons with C++ comparison operators. Since
  `base::span` comparison operators require both sides to be spans for template
  argument deduction to succeed, explicitly wrap both sides or use
  `std::ranges::equal`.

  ```cpp
  // After
  bool is_equal = (base::span(array1) == base::span(array2));
  ```

______________________________________________________________________

#### **8. Cross-Platform Struct-to-Span Conversions & Cleans**

- **Problem:** Zero-initializing or copying bytes of standard C structures (like
  `struct sockaddr_in` or `struct __res_state`) with padding bytes. Standard
  helpers like `base::as_writable_byte_span` or `base::byte_span_from_ref` fail
  compile-time unique-representation checks due to platform-specific layout
  differences or alignment padding.

- **Fix:** Bypass compile-time trait checks by using direct raw-pointer span
  creation wrapped in `UNSAFE_BUFFERS` and documented with a clear safety
  comment. Avoid using `base::allow_nonunique_obj` since it causes compilation
  failures on platforms where the representation *is* unique.

  ```cpp
  // Before
  res_state res = GetState();
  UNSAFE_TODO(memset(res, 0, sizeof(*res)));

  // After
  // SAFETY: `res` is a standard C struct pointer of size sizeof(*res), so
  // casting its dereferenced object to a byte span is safe.
  std::ranges::fill(UNSAFE_BUFFERS(base::span(reinterpret_cast<uint8_t*>(res),
                                             sizeof(*res))),
                    0);
  ```

______________________________________________________________________

#### **9. Safe POSIX Macro & Raw Array Wrapping**

- **POSIX Network Macros:** Low-level POSIX macros (e.g., `IN6_IS_ADDR_LOOPBACK`,
  `IN6_IS_ADDR_LINKLOCAL`) should not be wrapped in unsafe buffers. Instead,
  instantiate a safe `net::IPAddress` object from raw address bytes.

  ```cpp
  // Before
  if (UNSAFE_TODO(IN6_IS_ADDR_LOOPBACK(&addr->sin6_addr))) { ... }

  // After
  IPAddress ip_address(base::span(addr->sin6_addr.s6_addr));
  if (ip_address.IsLoopback()) { ... }
  ```

- **Raw Array Indexing:** Accessing raw C-style arrays with a runtime index
  (even in conditional blocks like `#if BUILDFLAG(...)`) causes unsafe buffer
  warnings. Always wrap the raw array in a safe `base::span` first.

  ```cpp
  // Before
  const char* const kServers[] = { ... };
  inet_pton(AF_INET, kServers[i], &sa.sin_addr);

  // After
  const char* const kServers[] = { ... };
  auto servers_span = base::span(kServers);
  inet_pton(AF_INET, servers_span[i], &sa.sin_addr);
  ```

- **Complex Character Array Sequences:** Avoid constructing spans from string
  literals containing embedded null characters (e.g.,
  `"chromium.org\0example.com"`). Declare them as explicit collections using
  `std::array` with Class Template Argument Deduction (CTAD) instead.

  ```cpp
  // Before
  const char kDnsrch[] = "chromium.org" "\0" "example.com";
  base::span(res.defdname).copy_from(kDnsrch);

  // After
  constexpr std::array kDnsrch = {
      'c', 'h', 'r', 'o', 'm', 'i', 'u', 'm', '.', 'o', 'r', 'g', '\0',
      'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm', '\0'
  };
  base::span(res.defdname).first(kDnsrch.size()).copy_from(kDnsrch);
  ```

- **Fuzzer Entrypoints (LibFuzzer):** Raw fuzzer inputs (`const uint8_t* data`,
  `size_t size` in `LLVMFuzzerTestOneInput`) trigger unsafe buffer warnings
  because the compiler cannot statically verify their bounds. Wrap fuzzer input
  span conversions in `UNSAFE_BUFFERS(...)` with a safety comment.

  ```cpp
  // Before
  extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    auto s = base::span(data, size);
  }

  // After
  extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // SAFETY: `data` and `size` are safely provided by the fuzzer framework,
    // so wrapping them in a span is safe.
    auto s = UNSAFE_BUFFERS(base::span(data, size));
  }
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
