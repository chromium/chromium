## GTest Discovery
1. If the user provided a ${file} you can make the following assumptions:
  - If the `${file}` is in the `chrome` folder and ends in `_unittest.cc`,
    the ${test_name} `unit_tests`.
  - If the `${file}` is in the `chrome` folder and ends in `_browsertest.cc`,
    the ${test_name} `browser_tests`.

2. If you were able to determine the `${test_name}` from the `${file}`, you can
  Read the file to extract a `${test_filter}` that would match all tests in the
  file.
  - For example if the file has `MyTestSuite.MyTest` and `MyTestSuite.MyTest2`,
    the `${test_filter}` can be `MyTestSuite.*`.

3. If you were able to determine the `${test_name}` and `${test_filter}`,
   `## GTest Discovery` has passed, otherwise it has failed.
