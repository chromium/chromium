# question-answering

https://huggingface.co/csarron/mobilebert-uncased-squad-v2

```python
from transformers import pipeline

qa_pipeline = pipeline(
    "question-answering",
    model="csarron/mobilebert-uncased-squad-v2",
    tokenizer="csarron/mobilebert-uncased-squad-v2"
)

predictions = qa_pipeline({
    'context': "The game was played on February 7, 2016 at Levi's Stadium in the San Francisco Bay Area at Santa Clara, California.",
    'question': "What day was the game played on?"
})

print(predictions)
# output:
# {'score': 0.71434086561203, 'start': 23, 'end': 39, 'answer': 'February 7, 2016'}

```

### Javascript
```js
chrome.Malabr.QuestionAnswerModel.init()
chrome.Malabr.QuestionAnswerModel.infer(
		{context: "", question: ""}, {},  (answer, error)=>{})
```

### CPP
```cpp
chrome.Malabr.QuestionAnswerModel.init()
- label define = QA_INIT


chrome.Malabr.QuestionAnswerModel.infer
- the arg[0] is a dictonary
- label define = QA_INFER

```


### Python
```python
QA_INIT

qa_pipeline = pipeline(
    "question-answering",
    model="csarron/mobilebert-uncased-squad-v2",
    tokenizer="csarron/mobilebert-uncased-squad-v2"
)

QA_INFER

predictions = qa_pipeline({
    'context': "The game was played on February 7, 2016 at Levi's Stadium in the San Francisco Bay Area at Santa Clara, California.",
    'question': "What day was the game played on?"
})

```