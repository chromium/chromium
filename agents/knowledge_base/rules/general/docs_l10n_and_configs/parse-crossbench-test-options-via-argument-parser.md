---
name: general/parse-crossbench-test-options-via-argument-parser
rubric: readability
severity: should_fix
---

**Rule.** In Python scripts and test runners, define and extract command-line
arguments using standard argument parsing libraries (such as
argparse.ArgumentParser) rather than ad-hoc extraction helpers, manual string
slicing, or manual argument list manipulation. When adding or modifying options
in scripts that support standard argument parsers, declare new arguments via
parser.add_argument() (and inspect parse_known_args() outputs) instead of
scanning or popping flags from passthrough argument lists.

**Why.** Ad-hoc argument parsing (such as .startswith(), splitting on '=', or
manually removing elements from argument lists) is brittle and fails on standard
CLI conventions like optional values, flag aliases, and '--flag value' vs.
'--flag=value' formats. Standard libraries like argparse provide automatic type
conversion, validation, default handling, and standard --help formatting.

## Examples

Bad:

```cpp
# Ad-hoc argument extraction and manual list manipulation:
def _is_alum(self):
  return _get_arg(self.options.passthrough_args, '--alum')

if http_port_arg := _get_arg(args, '--wpr-http-port='):
  self.options.passthrough_args.remove(http_port_arg)
  http_port = int(http_port_arg.split('=', 1)[1])
```

Good:

```cpp
# Register flags on ArgumentParser and read parsed attributes:
def _parse_arguments(self):
  parser = argparse.ArgumentParser()
  parser.add_argument('--alum', action='store_true', default=False)
  parser.add_argument('--wpr-http-port', type=int, help='HTTP port for WPR')
  self.cb_options, self.options.passthrough_args = parser.parse_known_args()

def _is_alum(self):
  return self.cb_options.alum
```
