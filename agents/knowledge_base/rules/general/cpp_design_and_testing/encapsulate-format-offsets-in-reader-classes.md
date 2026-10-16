---
name: general/encapsulate-format-offsets-in-reader-classes
rubric: design
severity: should_fix
---

**Rule.** Encapsulate binary format structure offsets and header positions
inside the parser or reader class rather than calculating them via manual
arithmetic in callers or tests. Expose dedicated offset or section accessor
helpers on the reader when callers need boundary or subspan locations.

**Why.** Computing internal layout offsets in callers or test code leaks
format-specific details outside the parser abstraction and duplicates brittle
arithmetic across call sites. Encapsulating offset logic inside the reader keeps
layout knowledge contained and simplifies caller interactions with buffers and
spans.

## Examples

Bad:

```cpp
const IMAGE_DOS_HEADER* dos_header = image_reader_.GetDosHeader();
const IMAGE_FILE_HEADER* file_header = image_reader_.GetCoffFileHeader();
const size_t section_headers_offset =
    static_cast<size_t>(dos_header->e_lfanew) +
    offsetof(IMAGE_NT_HEADERS, FileHeader) + sizeof(IMAGE_FILE_HEADER) +
    file_header->SizeOfOptionalHeader;
const size_t header_size =
    section_headers_offset +
    image_reader_.GetNumberOfSections() * sizeof(IMAGE_SECTION_HEADER);
```

Good:

```cpp
const size_t header_size =
    image_reader_.GetSectionHeaderOffsetAt(
        image_reader_.GetNumberOfSections() - 1) +
    sizeof(IMAGE_SECTION_HEADER);
```
