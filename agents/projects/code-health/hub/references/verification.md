# Shared Build & Test Verification Workflow

Use these instructions to verify compilation and execution of tests. Because
compilation and tests can take a long time and must run in the user's
environment, compilation or test commands MUST NOT be executed directly by the
agent. Proactively instruct the user to run the build compilation target and/or
relevant unit tests, then stop execution and wait for their confirmation.

To run these checks, dynamically determine the target and test names from the
modified files:

- **Build Target:** Analyze the paths of the modified files. Identify the
  appropriate build target (e.g., check `BUILD.gn` definitions or use a standard
  target like `chrome_apk` or `chrome_public_apk` for Clank/Chrome).
- **Test Class Name:** Check if any modified files are tests themselves, or
  search the directory for unit tests associated with the modified production
  files (e.g., `MyClassTest`). If no specific test is found, look for
  directory-level tests or use the automated test runner to identify related
  tests (e.g., using `--run-related` options).

1. **Build Verification:**
   - Proactively instruct the user to run the build compilation target to ensure
     no syntax or compilation errors were introduced:
     - Command: `autoninja -C <out_dir> <build_target>` (e.g., `chrome_apk` or
       `chrome_public_apk`)
2. **Unit Test Verification:**
   - If the modified files include unit tests or are covered by unit tests, ask
     the user to execute the unit tests to confirm they pass:
     - Command: `tools/autotest.py -C <out_dir> <test_class_name>`
