## Identify Test command
Based on the `${test_name}` and `${test_filter}`, determine the exact command
to run the test.
- For example: if the `${test_name}` is `browser_tests`, the command will be:
  `./${out_dir}/browser_tests --gtest_filter=${test_filter}`
- Based on the user's operating system, you may need to add `.exe` to the end of
  the executable name.
- Test filter may use the syntax `MyTestSuite.MyTest` or `MyTestSuite.*` to run
  all tests in the suite, this is useful for running all tests in a suite
  without having to specify each test individually.

## Run the tests and fix any runtime errors
You **must** run the test command to test the code after a successful compile.
- If you encounter any test or compile errors, make your best effort to fix
  them, then **return to ## Build and fix compile errors**.
- If you have failed to fix the same error output after 3 attempts, you should
  **stop** and let the user know that you are unable to fix the errors and that
  they should try to provide your more information, or fix them manually.

### Example Test Errors
If you encounter any test errors you could not fix in one try, that it would
have been helpful to have generic examples for, let the user know that
they can update this prompt to include this information in the future for faster
fixes.
