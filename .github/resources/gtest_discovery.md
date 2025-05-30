## GTest Discovery
1. If the user provided a `${file}` you can find a matching `${test_name}`
   with the following command: `gn refs out/{out_dir} ${file}` as `{gn_target}`
     - if the result is `//chrome/test:browser_tests`,
       `//chrome/test:browser_tests` is the `{gn_target}`.

2. Confirm if it is an executable using `gn desc out\{outdir} {gn_target} outputs`
     - If the response contains a test executable, then use that as the
       `{test_name}`.
     - If the response is similar to
       `Don't know how to display "outputs" for "source_set".`, you will need to
       identify the test that references this source set. You can do this with
       `gn refs --all out/{out_dir} {gn_target}`. Select the most likely test
       binary to use as the `{test_name}`.

3. If you were able to determine the `${test_name}` from the `${file}`, you can
   Read the file to extract a `${test_filter}` that would match all tests in the
   file.
   - For example if the file has `MyTestSuite.MyTest` and `MyTestSuite.MyTest2`,
     the `${test_filter}` can be `MyTestSuite.*`.

4. If you were able to determine the `${test_name}` and `${test_filter}`,
   `## GTest Discovery` has passed, otherwise it has failed.
