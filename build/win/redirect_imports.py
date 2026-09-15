#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Redirects a Windows PE image's imports to a forwarding DLL.

Every redirected import must resolve against the forwarding DLL's export
table, so a rewritten image cannot be produced with imports that would fail
at load time.
"""

import argparse
import ntpath
import os
import shutil
import sys

sys.path.insert(
    0,
    os.path.join(
        os.path.dirname(__file__), '..', '..', 'third_party', 'pefile_py3'
    ),
)
import pefile

_ARCHITECTURES = {
    'arm64': (
        pefile.OPTIONAL_HEADER_MAGIC_PE_PLUS,
        pefile.MACHINE_TYPE['IMAGE_FILE_MACHINE_ARM64'],
    ),
    'ia32': (
        pefile.OPTIONAL_HEADER_MAGIC_PE,
        pefile.MACHINE_TYPE['IMAGE_FILE_MACHINE_I386'],
    ),
    'x64': (
        pefile.OPTIONAL_HEADER_MAGIC_PE_PLUS,
        pefile.MACHINE_TYPE['IMAGE_FILE_MACHINE_AMD64'],
    ),
    'x86': (
        pefile.OPTIONAL_HEADER_MAGIC_PE,
        pefile.MACHINE_TYPE['IMAGE_FILE_MACHINE_I386'],
    ),
}


def _validate_basename(name, description):
    if (
        not name
        or os.path.basename(name) != name
        or ntpath.basename(name) != name
    ):
        raise ValueError(f'{description} must be a basename: {name!r}')


def _encode_import_name(import_dll_name):
    _validate_basename(import_dll_name, 'Import DLL name')
    if '\0' in import_dll_name:
        raise ValueError('Import DLL name must not contain a NUL')
    try:
        return import_dll_name.encode('ascii')
    except UnicodeEncodeError as error:
        raise ValueError('Import DLL name must be ASCII') from error


def _read_exports(import_dll):
    """Returns the names and ordinals exported by `import_dll`."""
    dll = pefile.PE(import_dll, fast_load=True)
    try:
        dll.parse_data_directories(
            directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']]
        )
        directory = getattr(dll, 'DIRECTORY_ENTRY_EXPORT', None)
        if not directory:
            raise RuntimeError(
                f'{os.path.basename(import_dll)} has no export table'
            )
        names = {symbol.name for symbol in directory.symbols if symbol.name}
        ordinals = {symbol.ordinal for symbol in directory.symbols}
        return names, ordinals
    finally:
        dll.close()


def _find_unresolved(redirected_imports, exported_names, exported_ordinals):
    """Returns the redirected imports the forwarding DLL does not export."""
    unresolved = set()
    for original_dll, imported_symbol in redirected_imports:
        source = original_dll.decode('ascii', errors='ignore')
        if imported_symbol.name:
            name = imported_symbol.name.decode('ascii', errors='ignore')
            if imported_symbol.name not in exported_names:
                unresolved.add(f'{source}!{name}')
        elif imported_symbol.ordinal not in exported_ordinals:
            unresolved.add(f'{source}!@{imported_symbol.ordinal}')
    return sorted(unresolved)


def redirect_imports(
    input_image,
    output_image,
    architecture,
    import_dll,
    skip_list=(),
    related_file=None,
):
    """Redirects non-skipped imports in `input_image` to `import_dll`.

    Args:
        input_image: Input PE image path.
        output_image: Rewritten PE image path.
        architecture: Chromium target CPU (`x86`, `x64`, or `arm64`).
        import_dll: Forwarding DLL path. Its basename is written into each
            redirected import entry, and its export table must cover every
            redirected symbol.
        skip_list: Case-insensitive DLL names to preserve.
        related_file: Optional explicit sidecar path to copy.

    Raises:
        RuntimeError: The image has no import table, has no eligible imports,
            or imports symbols the forwarding DLL does not export.
        ValueError: The architecture mismatches or the new name does not fit.
    """
    input_name = os.path.basename(input_image)
    import_dll_name = os.path.basename(import_dll)
    output_dir = os.path.dirname(output_image) or '.'
    _validate_basename(input_name, 'Input image name')
    _validate_basename(os.path.basename(output_image), 'Output image name')
    if os.path.normcase(os.path.abspath(input_image)) == os.path.normcase(
        os.path.abspath(output_image)
    ):
        raise ValueError('Input and output images must be different')
    related_name = None
    if related_file:
        related_name = os.path.basename(related_file)
        if not related_name.lower().startswith(input_name.lower() + '.'):
            raise ValueError(
                f'Related file must be named {input_name}.*: {related_file!r}'
            )
        related_destination = os.path.join(output_dir, related_name)
        if os.path.normcase(os.path.abspath(related_file)) == os.path.normcase(
            os.path.abspath(related_destination)
        ):
            raise ValueError('Related input and output files must be different')
    import_dll_bytes = _encode_import_name(import_dll_name)
    import_dll_lower = import_dll_bytes.lower()
    skipped_dlls = {
        name.strip().lower().encode('ascii')
        for name in skip_list
        if name.strip()
    }

    image = pefile.PE(input_image, fast_load=True)
    try:
        if architecture not in _ARCHITECTURES:
            raise ValueError(f'Unsupported architecture: {architecture}')
        expected_pe_type, expected_machine = _ARCHITECTURES[architecture]
        if (
            image.PE_TYPE != expected_pe_type
            or image.FILE_HEADER.Machine != expected_machine
        ):
            raise ValueError(
                f'{input_name} does not match architecture {architecture}'
            )

        image.parse_data_directories(
            directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_IMPORT']]
        )
        imports = getattr(image, 'DIRECTORY_ENTRY_IMPORT', None)
        if not imports:
            raise RuntimeError(f'{input_name} has no import table')

        eligible_count = 0
        redirected_imports = []
        for imported_dll in imports:
            original_name = imported_dll.dll
            original_name_lower = original_name.lower()
            if original_name_lower in skipped_dlls:
                print(f'Skipping {original_name.decode("ascii")}')
                continue
            eligible_count += 1
            redirected_imports.extend(
                (original_name, symbol) for symbol in imported_dll.imports
            )
            if original_name_lower == import_dll_lower:
                continue
            if len(import_dll_bytes) > len(original_name):
                raise ValueError(
                    f'{import_dll_name} does not fit over '
                    f'{original_name.decode("ascii")}'
                )

            image.set_bytes_at_rva(
                imported_dll.struct.Name, import_dll_bytes + b'\0'
            )
            print(
                f'Redirected {original_name.decode("ascii")} to '
                f'{import_dll_name}'
            )

        if eligible_count == 0:
            raise RuntimeError(f'{input_name} has no eligible imports')

        exported_names, exported_ordinals = _read_exports(import_dll)
        unresolved = _find_unresolved(
            redirected_imports, exported_names, exported_ordinals
        )
        if unresolved:
            raise RuntimeError(
                f'{import_dll_name} does not export every symbol '
                f'{input_name} imports:\n  ' + '\n  '.join(unresolved)
            )

        os.makedirs(output_dir, exist_ok=True)
        image.write(filename=output_image)
    finally:
        image.close()

    if related_name:
        shutil.copy(related_file, related_destination)
        print(f'Copied {related_file} to {related_destination}')


def _parse_arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input-image', required=True)
    parser.add_argument('--output-image', required=True)
    parser.add_argument(
        '--architecture', choices=sorted(_ARCHITECTURES), required=True
    )
    parser.add_argument(
        '--import-dll',
        required=True,
        help='Forwarding DLL whose exports must cover every redirected import',
    )
    parser.add_argument(
        '--skip',
        action='append',
        default=[],
        help='Case-insensitive DLL name to preserve',
    )
    parser.add_argument(
        '--related-file', help='Explicit <image-name>.* sidecar to copy'
    )
    return parser.parse_args(argv)


def main(argv):
    options = _parse_arguments(argv)
    redirect_imports(
        options.input_image,
        options.output_image,
        options.architecture,
        options.import_dll,
        skip_list=options.skip,
        related_file=options.related_file,
    )
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
