## GTest Discovery
1. If the user provided a `${file}` you can find a matching `${test_name}`
   with the following command:
  `gn refs --all --type=executable out/{out_dir} ${file}`
     - if the result is `//chrome/test:browser_tests`,
       `browser_tests` is the `{test_name}`.

2. If you were able to determine the `${test_name}` from the `${file}`, you can
   Read the file to extract a `${test_filter}` that would match all tests in the
   file.
   - For example if the file has `MyTestSuite.MyTest` and `MyTestSuite.MyTest2`,
     the `${test_filter}` can be `MyTestSuite.*`.

3. If you were able to determine the `${test_name}` and `${test_filter}`,
   `## GTest Discovery` has passed, otherwise it has failed.
