# Step to generate/update \*\_pb2.py stubs from proto

1. Install `protoc` if it is not in the `$PATH`
2. From this dir, Run the command `protoc --python_out=. *.proto` to generate
   python stubs
