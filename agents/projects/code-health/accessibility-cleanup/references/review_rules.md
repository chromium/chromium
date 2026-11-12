# Review Rules: Accessibility Cleanup

- **No Regressions:** Ensure that the end-user behavior has no regressions and
  is expected to improve from more accurate API usage.
- **Unused Strings:** Are there any strings that are now unused in the code? If
  so, ensure that the strings have been removed along with their corresponding
  png sha1 files if such files exist. For example, when removing the string
  `example_string`, which is declared in the file:
  `components/feature_strings.grdp` with the name `IDS_EXAMPLE_STRING`, there
  may be a corresponding file:
  `components/feature_strings_grdp/IDS_EXAMPLE_STRING.png.sha1`, which should be
  deleted.
- **Duplicate Code:** Check that we are not creating duplicate code with a
  reusable component or utility method/class that exists in the codebase.
