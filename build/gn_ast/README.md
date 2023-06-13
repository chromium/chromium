# GN AST

A Python library for working with GN files via abstract syntax tree (AST).

## JNI Refactor Example

This library was originally created to perform the refactor within
`jni_refactor.py`. The file is left as an example.

```sh
# To apply to all files:
find -name BUILD.gn > file-list.txt
# To apply to those that match a pattern:
grep -r --files-with-matches --include "BUILD.gn" "some pattern" > file-list.txt

# To run one-at-a-time:
for f in $(cat file-list.txt); do python3 jni_refactor.py "$f"; done
# To run in parallel:
parallel python3 jni_refactor.py -- $(cat file-list.txt)
```
