---
name: experimental-test-suite-mapper
description: >-
  Finds the executable test suites (e.g., `browser_tests`, `unit_tests`)
  responsible for compiling and executing a specific target source code file or
  test file in Chromium on Linux.
---

# Test Suite Mapper Skill

This skill provides a fast, reliable, and deterministic way to map a Chromium
source or test file to its parent executable test suite on Linux. It uses static
dependency analysis of the GN build graph.

______________________________________________________________________

## When to Use This Skill

Use this skill when:

- The target build configuration is Linux (`target_os = "linux"`).
- Triaging an underreported coverage bug on Linux and you need to identify what
  test suite should contain the developer's test.
- Investigating a C++ or Web/WPT code change on Linux and you need to identify
  what tests cover those files.
- Determining if a newly added test is compiled into a runnable test suite on
  Linux.

______________________________________________________________________

## How to Use This Skill

Depending on what information you have, use one of the two workflows below:

### Workflow A: You have a Test Name (e.g., `URLParser.PathUrl`)

If the bug description or comments explicitly mention a test name or GTest
filter:

1. **Locate the Test File:** Search the codebase for the test definition using a
   case-insensitive search.

   *Example search pattern:* `TEST.*URLParser.*PathUrl` *Example resolved file:*
   `url/url_parse_unittest.cc`

2. **Map the Test File to the Suite:** Determine which executable test suites
   compile the test file. *Example using the helper tool:*

   ```bash
   python3 tools/code_coverage/test_suite_mapper.py url/url_parse_unittest.cc
   ```

   *Example output:*

   ```json
   [
     "url_unittests"
   ]
   ```

*Note: This workflow is preferred because test files are typically compiled into
only one (or very few) test runner targets, avoiding proximity noise.*

### Workflow B: You only have a Source File / Code Area

If you only know which source file has missing coverage:

1. **Map the Source File directly:** Determine which executable test suites
   compile the source file. *Example using the helper tool:*

   ```bash
   python3 tools/code_coverage/test_suite_mapper.py base/values.cc
   ```

   *Example output:*

   ```json
   [
     "base_perftests",
     "base_unittests"
   ]
   ```

2. **Interpret the Results:** A path proximity filter is used to narrow down the
   consuming test suites. For core files (like `base/`), the most relevant
   suites in the same directory are preferred. For files in `chrome/`, a list of
   several covering suites (e.g., `browser_tests`, `unit_tests`) may be returned
   because they all depend on it.

### Customizing the Build Directory

By default, the build directory is assumed to be `out/Default`. You can specify
a custom build directory (which must contain `args.gn` and be configured for
Linux): *Example using `--build-dir`:*

```bash
python3 tools/code_coverage/test_suite_mapper.py \
  --build-dir out/coverage base/values.cc
```

