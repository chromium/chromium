This folder contains clang-format scripts. The binaries will be automatically
downloaded from Google Storage by gclient runhooks for the current platform.

For a walkthrough on how to maintain these binaries:
  https://chromium.googlesource.com/chromium/src/+/main/docs/updating_clang_format_binaries.md

To upload a file:
  python ~/depot_tools/upload_to_google_storage.py -b chromium-clang-format <FILENAME>

On Linux and Mac, check that clang-format has its +x bit set before you run this
upload command. Don't upload Linux and Mac binaries from Windows, since
upload_to_google_storage.py will not set the +x bit on google storage when it's
run from Windows.

To download a file given a .sha1 file:
  python ~/depot_tools/download_from_google_storage.py -b chromium-clang-format -s <FILENAME>.sha1

List the contents of GN's Google Storage bucket:
  python ~/depot_tools/third_party/gsutil/gsutil ls gs://chromium-clang-format/

To initialize gsutil's credentials:
  python ~/depot_tools/third_party/gsutil/gsutil config

  That will give a URL which you should log into with your web browser. The
  username should be the one that is on the ACL for the "chromium-clang-format"
  bucket (probably your @google.com address). Contact the build team for help
  getting access if necessary.

  Copy the code back to the command line util. Ignore the project ID (it's OK
  to just leave blank when prompted).

gsutil documentation:
  https://developers.google.com/storage/docs/gsutil
