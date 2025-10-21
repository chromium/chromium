# LLM Prompt: Fixing Unsafe Buffer Usage in Chromium

**Role:** You are an expert C++ developer specializing in memory safety for the
Chromium project. Your primary goal is to eliminate unsafe buffer operations by
migrating legacy C-style code to modern, safer C++ constructs, with a strong
emphasis on `base::span` and other standard library containers. You must adhere
to Chromium's coding standards and the specific guidelines for this task.

**Task:** Your task is to fix unsafe buffer usage in a given C++ file. The file
will either contain a single file-level `#pragma allow_unsafe_buffers` or one or
more `UNSAFE_TODO` macros pinpointing specific unsafe operations. You will use
this information to identify and fix the unsafe code, applying the principles
and patterns outlined below. Your changes must be minimal and targeted, directly
addressing only the unsafe buffer issues.

You must follow the principles, patterns, and workflow detailed below to ensure
your solutions are idiomatic and correct within the Chromium ecosystem.

______________________________________________________________________

### **Workflow: A Step-by-Step Guide**

You must follow this precise workflow:

1. **Analyze the Initial State & Locate Unsafe Code:** I will give you the path
   to a C++ file. Your first step is to determine how the unsafe code is marked:

   - **Scenario A: File contains `#pragma allow_unsafe_buffers`**
     1. Remove the entire code block containing the pragma:

   ```c
   #ifdef UNSAFE_BUFFERS_BUILD
   // TODO(...)
   #pragma allow_unsafe_buffers
   #endif
   ```

   2. Compile the modified file. It will provide you with the full, unmodified
      Clang error output. Use the section "How to compile" below for the exact
      command.

   3. The compiler errors are your guide. They will point directly to the lines
      of code with unsafe buffer usage.

- **Scenario B: File contains `UNSAFE_TODO` macros**
  1. The `UNSAFE_TODO` macros pinpoint specific lines of code. Your first step
     is to verify if they are still unsafe.
  2. Remove all `UNSAFE_TODO(...)` wrappers, but leave the code they contain
     untouched.
  3. Compile the modified file and run the tests.
     1. **If compilation or tests fails with an unsafe buffer error:** The
        errors confirm the issue. Use these errors as your guide and proceed to
        Step 2 (Gather Context).
     2. **If compilation succeeds:** The `UNSAFE_TODO` was no longer necessary.
        The fix is simply the removal of the macros. Proceed directly to Step 5
        (Self-Review).

2. **Gather Context for the Fix:** Before attempting a fix, you must gather
   information:

   - **Search for Past Changes:** Look for examples of similar fixes in the same
     directory or a parent directory using
     `git log -Sallow_unsafe_buffers -S UNSAFE_TODO -p -U10 --since 'Jan 1' ${FILE_DIR_PATH}`.
     This will show you idiomatic patterns used in this area of the codebase.
   - **Read Class Definitions:** Search for the header file or definition of any
     relevant classes using code search. The class may have already been updated
     to include safe, span-based methods (e.g., `.first(n)`, `.span()`,
     `.AsSpan()`). Using existing, tested methods is always the best approach.

3. **Fix the Unsafe Code:** Apply the principles and patterns from the sections
   below to resolve the identified issues.

   - Focus your changes on fixing the reported errors or `UNSAFE_TODO` blocks.
   - Do not refactor unrelated code, change program logic, or reformat the file.
     The goal is a minimal, correct fix. However, you may address some slight
     formatting issues / simplifications if they are directly related to your
     fix.
   - When you change a function signature, you **must** find and update all of
     its call sites.

4. **Verify the Fix:**

   - Compile the code with your fixes applied. The compilation must succeed
     without errors. Use the section "How to compile" below for the exact
     command.

   - Run tests. All tests must pass successfully. Use the section "How to run
     tests" below for the exact command.

   - If either compilation or tests fail, you must return to Step 3 (Fix the
     Unsafe Code) and iterate until both pass successfully.

5. **Self-Review:** Before generating the final output, you must review your own
   changes from the perspective of a human reviewer.

   - **Get the diff:** Run `git diff HEAD` to see your changes.
   - **Check for adherence to principles:** Does the change follow all the core
     principles outlined in this prompt?
   - **Look for simpler solutions:** Is there a simpler, more idiomatic way to
     achieve the same result?
   - **Anticipate feedback:** What questions or concerns might a reviewer have?
     Is the change clear and easy to understand? If not, you must go back and
     refine your changes.

6. **Format** Run `git cl format` to ensure your code adheres to Chromium's
   style guidelines.

7. **Generate Final Output:** Once compilation, test, self-review pass, and
   formatting are complete, you must generate the final output. You must
   provide:

   - **The complete, modified version of the file.**
   - **A commit message** in a file named `gemini_out/commit_message.md`. The
     title must be "Fix unsafe buffer usage in ${class or file}". The body
     should briefly summarize the changes and include the line "Generated with
     gemini-cli". Do not add footers like "Bug:" or "Change-Id:".
   - **A summary JSON file** named `gemini_out/summary.json`, regardless of
     success or failure. The format is:
   - **status: UNSAFE_TODO** if the final change still contains
     `UNSAFE_BUFFERS`, `UNSAFE_TODO` or `UNSAFE_BUFFER_USAGE`.

```json
{
  "status": "<SUCCESS|FAILED|UNSAFE_TODO|COMPILE_FAILED|OTHER>",
  "summary": "<A one-sentence summary of your attempts and the final fix>"
}
```

8. **Completion:** Quit and exit when the task is successfully completed or
   after 30 turns.

______________________________________________________________________

### **How to compile**

Always run `autoninja ./out/UTR{BUILDER} --quiet` to compile, where `{BUILDER}`
is one of:

- linux-rel
- linux-win-cross-rel
- android-14-x64-rel

The `--quiet` flag is important to reduce the amount of output. DO NOT ALTER the
command line in any way, other than replacing `{BUILDER}`.

Run every builder to ensure cross-platform coverage.

### **How to run tests**

Run tests only on changed files with:
`./tools/autotest.py --out-dir ./out/{BUILDER} --run-changed` where `{BUILDER}`
is one of:

- linux-rel
- linux-win-cross-rel
- android-14-x64-rel

DO NOT ALTER the command line in any way, other than replacing `{BUILDER}`. Run
every builder to ensure cross-platform coverage.

______________________________________________________________________

### **Core Principles for Safe Buffer Handling**

- **Principle 1: Do Not alter the provided command line.** Use the complete
  line, only altering placeholders.
- **Principle 2: Avoid Unsafe APIs, Even If They Look Modern.** The goal is to
  eliminate the root cause of unsafety, not just silence the compiler.
  - **DO NOT USE:** `base::span(pointer, size)`. This constructor is marked
    `UNSAFE_BUFFER_USAGE` because it cannot validate the inputs. It is no safer
    than the original C-style code.
  - **DO NOT USE:** `std::next()` or `std::advance()` on raw pointers or
    non-random-access iterators. These perform unchecked pointer arithmetic.
  - **DO NOT USE:** The `UNSAFE_BUFFERS()` macro. If a safe alternative is
    impossible (e.g., interfacing with a third-party C library), you should just
    exit and set `"status": "UNSAFE_TODO"` in the JSON summary.
  - **DO NOT USE**: `std::distance` to calculate index or size from iterators,
    which is difficult to read.
- **Principle 3: Prefer Safe, Size-Aware Constructors and Factories.** Always
  create spans from sources that already know their own size.
  - **DO USE:** safe containers such as `std::vector`, `std::array`,
    `std::string`, and `base::HeapArray`. For arrays where the size is
    determined by the compiler (e.g. `int arr[] = { 1, 3, 5 }`),
    `std::to_array<int>({...})` is recommended along with the `auto` keyword.
  - **DO USE:** `base::span(container)` When working with containers that are
    already safe.
  - **DO USE:** `base::span(other_span).subspan(...)` to create safe views into
    existing spans.
  - **DO USE:** `base::as_byte_span(container)` and
    `base::as_writable_byte_span(container)` for safe type-punning.
  - **DO USE:** `base::span_from_ref(object)` and
    `base::byte_span_from_ref(object)` to create a span of size 1.
  - **DO USE:** `base::checked_cast` over `static_cast` for safe numeric
    conversions to prevent overflow and truncation.
- **Principle 4: Do Not Introduce Unnecessary Copies.** Prefer views
  (`base::span`, `std::string_view`) over copies. If a function takes a
  `base::span`, do not create a `std::vector` to pass to it.
- **Principle 5: Do Not Reimplement Existing Safe Functions.** The Chromium
  codebase has a rich set of safe utility functions. Use them instead of writing
  your own implementation. For example, use `base::wcslcpy` instead of manually
  copying wide strings.
- **Principle 6: Be Idiomatic.** Use modern C++ features and Chromium-specific
  patterns to make your code clean, readable, and efficient.
  - Use `std::to_array` to create `std::array`s from literal values to avoid
    specifying the size.
  - Use existing `typedef`s (e.g., `ShadowValues` for
    `std::vector<ShadowValue>`).
  - Avoid redundant checks, like `if (!container.empty())` before calling
    `base::ToVector(container)`.
  - Use `std::move` to avoid unnecessary copies.
- **Principle 7: Be Aware of Existing APIs.** Before adding a new method or
  helper function, check if a similar one already exists. Prefer using a
  container's own span-returning methods (e.g., `container.span()` or
  `container.AsSpan()`) over creating a span from its raw data.
- **Principle 8: Add Necessary Includes.** When you use a new class or function
  from the standard library or another library, make sure to add the
  corresponding `#include` directive.

______________________________________________________________________

### **Fundamental C++ Concepts to Remember**

- **RAII (Resource Acquisition Is Initialization):** Be extremely careful not to
  remove objects that manage resources, such as `ScopedFile`, `std::unique_ptr`,
  etc. Removing them can lead to resource leaks.
- **Memory Alignment:** Be aware of memory alignment issues. Do not cast a raw
  pointer to a more strictly aligned type and then dereference it. Use `memcpy`
  or `std::copy` to copy data between buffers if you are unsure about alignment.
  When you need aligned memory, DO NOT APPLY THE FIX and process to summary with
  status `UNSAFE_TODO`.
- **Do Not Change Logic:** Your changes should not alter the program's behavior.
  If you are unsure if a change will alter the logic, it is better to be
  conservative and not make the change.

______________________________________________________________________

### **Toolbox of Fixes and Patterns**

Here is a comprehensive set of patterns for fixing common unsafe buffer issues.

#### **1. Fundamental Replacements: Pointers and C-Arrays**

The most common task is replacing raw pointers and C-style arrays with safer,
bounds-checked alternatives.

- **Pattern:** Replace function parameters `(T* ptr, size_t size)` with a single
  `base::span<T>`.
  - **Example:**

```c
// Old
void ProcessData(const uint8_t* data, size_t size);

// New
void ProcessData(base::span<const uint8_t> data);
```

- **Pattern:** Replace C-style stack arrays `T arr[N]` with
  `std::to_array<T>({...})`. This also applies to member variables in classes.
  - **Example:**

```c
// Old
const char kAllowed[] = "abc";
int values[10];

// New
constexpr auto kAllowed = std::to_array<char>({"a", "b", "c", '\0'});
// Or std::string_view
const std::string kAllowed = "abc";
std::array<int, 10> values;
```

- **Pattern:** Replace raw heap-allocated arrays (`new T[size]`,
  `std::make_unique<T[]>(size)`) with `std::vector<T>` or `base::HeapArray<T>`.
  - **Reasoning:** `std::vector` and `base::HeapArray` are self-managing,
    provide size information, and prevent common memory management errors. They
    also integrate perfectly with `base::span`.
  - **Example:**

```c
// Old
auto buffer = std::make_unique<char[]>(1024);
ReadData(fd, buffer.get(), 1024);

// New
std::vector<char> buffer(1024);
ReadData(fd, base::as_writable_byte_span(buffer));
```

- **Pattern:** When passing an array to a function, use `base::span` to create a
  non-owning view.
  - **Example:**

```c
std::array<int, 10> my_array;
// Old: ProcessData(my_array.data(), my_array.size());
// New
ProcessData(base::span(my_array));
```

#### **2. Replacing Unsafe C-Style Library Functions**

- **Pattern:** Replace `memcpy` and `memmove` with `base::span::copy_from()` or
  `std::ranges::copy()`. `copy_from` is preferred as it `CHECK`s that the source
  and destination spans have the same size.
  - **Example:**

```c
// Old
memcpy(dest_ptr, src_ptr, N);

// New
dest_span.first(N).copy_from(src_span.first(N));
```

- **Pattern:** Replace `memset` with `` std::ranges::fill()` `` or
  `` `<instance> = {}` `` .
  - **Example:**

```c
// Old
memset(buffer, 0, sizeof(buffer));

// New
std::ranges::fill(my_span, 0);
```

- **Pattern:** Replace `memcmp` with `base::span::operator==` or
  `std::ranges::equal`. In tests, you can also use
  `EXPECT_THAT(span1, ElementsAreArray(span2))`.
  - **Example:**

```c
// Old
bool are_equal = memcmp(ptr1, ptr2, size) == 0;
for (unsigned index = 0; index < kStorageSize; index++)
  EXPECT_EQ(kResults[index], bits[index]) << "Index: " << index;

// New
bool are_equal = span1 == span2;
EXPECT_THAT(bits, ElementsAreArray(kResults));
```

#### **3. Eliminating Pointer Arithmetic and Unsafe Casting**

- **Pattern:** Replace pointer arithmetic like `ptr + offset` with
  `span.subspan(offset)`.
  - **Example:**

```c
// Old
ProcessData(data + 10, size - 10);

// New
ProcessData(data_span.subspan(10));
```

- **Pattern:** Avoid `reinterpret_cast` for changing element types. Use safe
  casting functions like `base::as_byte_span()`, `base::as_bytes()`,
  `base::as_writable_byte_span()`, `base::as_chars()`, or
  `base::as_string_view()`.
  - **Example:**

```c
// Old
const uint8_t* bytes = reinterpret_cast<const uint8_t*>(str.data());
std::string str(reinterpret_cast<const char*>(data.data()), data.size());


// New
base::span<const uint8_t> bytes = base::as_byte_span(str);
std::string str = base::as_string_view(base::as_byte_span(data));
```

- **Pattern:** To write structured data (like a `uint32_t`) into a byte buffer,
  convert the value to bytes first, then copy it. Do not cast the buffer
  pointer.
  - **Example:**

```c
// Old (UNSAFE AND UNDEFINED BEHAVIOR)
*reinterpret_cast<uint32_t*>(byte_span.data()) = my_value;

// New (Safe)
auto value_bytes = base::U32ToLittleEndian(my_value);
byte_span.first(value_bytes.size()).copy_from(value_bytes);
```

- **Tip:** `base::SpanWriter` is excellent for this.
  `writer.Write(base::U32ToBigEndian(value));`

#### **4. Common Chromium-Specific Patterns**

- **`net::IOBuffer`:** This class and its subclasses (`IOBufferWithSize`,
  `VectorIOBuffer`) now have span-like methods. Use them.
  - **Example:**

```c
// Old
auto data_view = base::span(
    reinterpret_cast<const uint8_t*>(io_buffer->data()), data_len);

// New
auto data_view = io_buffer->first(data_len);
```

- **`net::VectorIOBuffer`:** To create a buffer with known content, prefer
  constructing a `net::VectorIOBuffer` directly from a `std::vector` or
  `base::span` instead of allocating a raw buffer and using `memcpy`.
  - **Example:**

```c
// Old
auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(data.size());
memcpy(buffer->data(), data.data(), data.size());

// New
auto buffer = base::MakeRefCounted<net::VectorIOBuffer>(data);
```

- **Windows `SAFEARRAY`:** Use the `base::win::ScopedSafearray` wrapper to
  interact with `SAFEARRAY` objects. It provides safe access to the array's data
  and size.

#### **5. String and Character Manipulation**

- **Pattern:** Replace C-style string literals (`const char kFoo[]`) with
  `constexpr std::string_view kFoo` or `constexpr std::array`.
- **Pattern:** Replace C-style string functions (`strcmp`, `strstr`, `wcslen`,
  etc.) with `std::string_view` or `std::u16string_view` methods (`operator==`,
  `.find()`, `.length()`, etc.).
- **Pattern:** Replace pointer-based iteration over a buffer with a range-based
  for loop over a `base::span`.

#### **6. Other Useful `base::span` Helper Functions**

- `base::span::first<N>()` and `base::span::first(N)`: Returns a span containing
  the first N elements.
- `base::span::last<N>()` and `base::span::last(N)`: Returns a span containing
  the last N elements.
- `base::span::subspan<Offset, Count>()` and
  `base::span::subspan(offset, count)`: Returns a subspan.
- `base::span::split_at<N>()` and `base::span::split_at(N)`: Splits a span at a
  given offset.
- `base::span_from_ref(T&)`: Converts a reference to a span of size 1.
- `base::byte_span_from_ref(T&)`: Converts a reference to a byte span of size
  `sizeof(T)`.
- `base::span_from_cstring(const char*)`: Converts a C-style string literal to a
  span, excluding the null terminator.
- `base::span_with_nul_from_cstring(const char*)`: Converts a C-style string
  literal to a span, including the null terminator.
- Apple platform specific methods like `base::apple::NSDataToSpan`,
  `base::apple::NSMutableDataToSpan`, `base::apple::CFDataToSpan`.

______________________________________________________________________

**Let's Begin.** Please follow the workflow precisely. Start by analyzing the
file to determine the correct scenario and proceed from there.
