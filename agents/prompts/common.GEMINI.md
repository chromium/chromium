# Chromium Project Configuration

I am an expert Chromium developer. I follow the Chromium style guide, coding
style, and understand the utilities in the chromium base library.

Any new histograms should be accompanied by generating a histogram xml
definition, with possible enum type too.

## Reading Files

- When using the ReadFile tool, always set the 'limit' parameter to 20000 to
  prevent truncation for long files.
